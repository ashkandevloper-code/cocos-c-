#include "backend/OnlineService.h"

#include "NxConfig.generated.h"
#include "core/text/PersianShaper.h"

#include <algorithm>
#include <cctype>

namespace nx {

OnlineConfig OnlineConfig::fromBuild() {
    OnlineConfig c;
    c.supabaseUrl = NX_CFG_SUPABASE_URL;
    c.supabaseAnonKey = NX_CFG_SUPABASE_ANON_KEY;
    c.gamesUrl = NX_CFG_GAMES_SUPABASE_URL;
    c.gamesAnonKey = NX_CFG_GAMES_SUPABASE_ANON_KEY;
    c.liveKitUrl = NX_CFG_LIVEKIT_URL;
    return c;
}

static std::string trim(const std::string& s) {
    size_t a = 0, b = s.size();
    while (a < b && std::isspace(static_cast<unsigned char>(s[a]))) ++a;
    while (b > a && std::isspace(static_cast<unsigned char>(s[b - 1]))) --b;
    return s.substr(a, b - a);
}
static std::string lower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return s;
}

OnlineService::OnlineService(const OnlineConfig& cfg, IHttp* http, IKeyValueStore* store, WebSocketFactory ws, Clock clock)
    : cfg_(cfg), clock_(clock ? std::move(clock) : Clock(systemNowMs)) {
    if (!cfg_.enabled()) return;
    // Same storage key family as supabase_flutter (sb-<ref>-auth-token), so a fresh install starts clean.
    main_.reset(new SupabaseClient(cfg_.supabaseUrl, cfg_.supabaseAnonKey, http, store, "nx.sb.auth.session", clock_));
    rtMain_.reset(new RealtimeClient(cfg_.supabaseUrl, cfg_.supabaseAnonKey, ws, clock_));
    if (cfg_.gamesEnabled()) {
        games_.reset(new SupabaseClient(cfg_.gamesUrl, cfg_.gamesAnonKey, http, nullptr, "", clock_));
        SupabaseClient* m = main_.get();
        games_->setAccessTokenProvider([m]() { return m->hasSession() ? m->session().accessToken : std::string(); });
        rtGames_.reset(new RealtimeClient(cfg_.gamesUrl, cfg_.gamesAnonKey, ws, clock_));
    }
    main_->onAuthStateChange([this](AuthEvent ev, const AuthSession& s) { onAuth(ev, s); });
    main_->restoreSession();
}

void OnlineService::onAuth(AuthEvent ev, const AuthSession& s) {
    status_ = s.valid() ? BackendStatus::Online : BackendStatus::Offline;
    std::string tok = s.valid() ? s.accessToken : "";
    if (rtMain_) rtMain_->setAuth(tok);
    if (rtGames_) rtGames_->setAuth(tok);
    if (s.valid() && (ev == AuthEvent::SignedIn || ev == AuthEvent::InitialSession || ev == AuthEvent::UserUpdated)) {
        // InitialSession restores from disk without network: refresh first if the token is stale.
        main_->ensureFreshToken([this](bool ok) { if (ok) gamesHello(false); });
    }
    if (!s.valid()) helloFor_.clear();
    notify();
}

void OnlineService::notify() {
    auto ls = listeners_;
    for (auto& l : ls) l.second();
}

bool OnlineService::isInternalAccount() const {
    std::string e = email();
    std::string suffix = std::string("@") + internalDomain;
    return e.size() >= suffix.size() && e.compare(e.size() - suffix.size(), suffix.size(), suffix) == 0;
}

std::string OnlineService::signupUsername(const std::string& username) {
    std::string c = trim(username);
    size_t runes = text::utf8To32(c).size();
    return (runes >= 3 && runes <= 20) ? c : "";
}

std::string OnlineService::normalizeEmailOtpDigits(const std::string& v) { return text::normalizeDigits(v); }

bool OnlineService::isSixDigitEmailOtp(const std::string& v) {
    if (v.size() != 6) return false;
    for (char c : v) if (c < '0' || c > '9') return false;
    return true;
}

bool OnlineService::isUsernameLogin(const std::string& id) { return trim(id).find('@') == std::string::npos; }
std::string OnlineService::loginEmail(const std::string& id) {
    return isUsernameLogin(id) ? lower(trim(id)) + "@" + internalDomain : trim(id);
}
std::string OnlineService::loginPassword(const std::string& id, const std::string& password) {
    return isUsernameLogin(id) ? "nx#" + password : password;
}

int OnlineService::otpCooldownLeft(const std::string& email) const {
    auto it = otpSentAt_.find(lower(trim(email)));
    if (it == otpSentAt_.end()) return 0;
    int64_t left = 60 - (clock_() - it->second) / 1000;
    return left > 0 ? static_cast<int>(left) : 0;
}

void OnlineService::runAuth(std::function<void(SbCallback)> op, std::function<void(bool)> cb) {
    if (!main_) {
        lastError_ = "not_configured";
        if (cb) cb(false);
        return;
    }
    busy_ = true;
    lastError_.clear();
    notify();
    op([this, cb](const SbResult& r) {
        busy_ = false;
        if (!r.ok) {
            SbResult n = r;
            SupabaseClient::normalizeAuthError(n);
            lastError_ = n.error;
            lastRetryAfter_ = n.retryAfter;
        }
        notify();
        if (cb) cb(r.ok);
    });
}

void OnlineService::sendEmailOtp(const std::string& email, std::function<void(bool)> cb) {
    std::string e = trim(email);
    lastRetryAfter_ = -1;
    runAuth([this, e](SbCallback done) { main_->signInWithOtp(e, true, emailRedirect, done); },
            [this, e, cb](bool ok) {
                if (ok) otpSentAt_[lower(e)] = clock_();
                if (cb) cb(ok);
            });
}

void OnlineService::verifyEmailOtp(const std::string& email, const std::string& token, std::function<void(bool)> cb) {
    std::string e = trim(email), t = normalizeEmailOtpDigits(token);
    runAuth([this, e, t](SbCallback done) { main_->verifyOtp(e, t, "email", done); },
            [this, cb](bool ok) { if (cb) cb(ok && main_->hasSession()); });
}

void OnlineService::registerEmail(const std::string& email, const std::string& password, const std::string& username,
                                  std::function<void(bool)> cb) {
    std::string safe = signupUsername(username);
    std::string e = trim(email);
    runAuth(
        [this, e, password, safe](SbCallback done) {
            if (main_->hasSession() && main_->session().isAnonymous) {
                main_->updateUser(Json::obj({{"email", e}, {"password", password}}), done);
            } else {
                main_->signUp(e, password, safe.empty() ? Json() : Json::obj({{"username", safe}}), done);
            }
        },
        cb);
}

void OnlineService::signInEmail(const std::string& idOrEmail, const std::string& password, std::function<void(bool)> cb) {
    std::string e = loginEmail(idOrEmail), p = loginPassword(idOrEmail, password);
    runAuth([this, e, p](SbCallback done) { main_->signInWithPassword(e, p, done); }, cb);
}

void OnlineService::changePassword(const std::string& newPassword, std::function<void(bool)> cb) {
    std::string pw = isInternalAccount() ? "nx#" + newPassword : newPassword;
    runAuth(
        [this, pw](SbCallback done) {
            main_->updateUser(Json::obj({{"password", pw}}), [this, done](const SbResult& r) {
                if (!r.ok) { done(r); return; }
                main_->refreshSession(done);  // clears the must-change-password claim from the JWT
            });
        },
        cb);
}

void OnlineService::resetPassword(const std::string& email, std::function<void(bool)> cb) {
    std::string e = trim(email);
    runAuth([this, e](SbCallback done) { main_->resetPasswordForEmail(e, done); }, cb);
}

void OnlineService::signOut(std::function<void()> cb) {
    if (!main_) { if (cb) cb(); return; }
    runAuth([this](SbCallback done) { main_->signOut(done); }, [cb](bool) { if (cb) cb(); });
}

void OnlineService::gamesHello(bool force, std::function<void(bool)> cb) {
    std::string id = uid();
    if (!games_ || id.empty() || (!force && helloFor_ == id)) {
        if (cb) cb(!id.empty());
        return;
    }
    games_->rpc("nx_g_hello", Json::object(), [this, id, cb](const SbResult& r) {
        if (r.ok) helloFor_ = id;
        if (cb) cb(r.ok);
    });
}

void OnlineService::grpc(const std::string& fn, const Json& params, SbCallback cb) {
    if (!main_ || !main_->hasSession()) {
        SbResult r;
        r.error = "offline";
        if (cb) cb(r);
        return;
    }
    gamesHello(false, [this, fn, params, cb](bool) { games()->rpc(fn, params, cb); });
}

void OnlineService::rpc(const std::string& fn, const Json& params, SbCallback cb) {
    if (!main_ || !main_->hasSession()) {
        SbResult r;
        r.error = "offline";
        if (cb) cb(r);
        return;
    }
    main_->rpc(fn, params, cb);
}

void OnlineService::tick(int64_t nowMs) {
    if (rtMain_) rtMain_->tick(nowMs);
    if (rtGames_) rtGames_->tick(nowMs);
}

}  // namespace nx
