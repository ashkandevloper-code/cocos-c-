// Port of lib/core/online/online_service.dart: auth + connection state for the whole app, two Supabase
// projects (main = auth/social, games = rooms/matches/tournaments trusting main's JWT).
#pragma once
#include "backend/Realtime.h"
#include "backend/SupabaseClient.h"

#include <functional>
#include <map>
#include <memory>
#include <string>

namespace nx {

struct OnlineConfig {
    std::string supabaseUrl, supabaseAnonKey;
    std::string gamesUrl, gamesAnonKey;
    std::string liveKitUrl;
    bool enabled() const { return !supabaseUrl.empty() && !supabaseAnonKey.empty(); }
    bool gamesEnabled() const { return !gamesUrl.empty() && !gamesAnonKey.empty(); }
    /// Values compiled in from NxConfig.generated.h.
    static OnlineConfig fromBuild();
};

enum class BackendStatus { Offline, Connecting, Online };

class OnlineService {
public:
    OnlineService(const OnlineConfig& cfg, IHttp* http, IKeyValueStore* store, WebSocketFactory ws, Clock clock = nullptr);

    bool configured() const { return main_ != nullptr; }
    BackendStatus status() const { return status_; }
    bool isOnline() const { return status_ == BackendStatus::Online && main_ && main_->hasSession(); }
    std::string uid() const { return main_ && main_->hasSession() ? main_->session().userId : ""; }
    std::string email() const { return main_ && main_->hasSession() ? main_->session().email : ""; }
    bool isGuest() const { return !main_ || !main_->hasSession() || main_->session().isAnonymous; }
    bool isInternalAccount() const;
    bool busy() const { return busy_; }
    const std::string& lastError() const { return lastError_; }
    int lastRetryAfter() const { return lastRetryAfter_; }
    const OnlineConfig& config() const { return cfg_; }

    SupabaseClient* client() { return main_.get(); }
    /// Games project client; falls back to the auth client when the games config is missing.
    SupabaseClient* games() { return games_ ? games_.get() : main_.get(); }
    RealtimeClient* realtime() { return rtMain_.get(); }
    RealtimeClient* gamesRealtime() { return rtGames_ ? rtGames_.get() : rtMain_.get(); }

    /// Returns an id for removeListener (scenes must remove theirs in onExit).
    int addListener(std::function<void()> l) { listeners_.emplace_back(++nextListener_, std::move(l)); return nextListener_; }
    void removeListener(int id) {
        for (auto it = listeners_.begin(); it != listeners_.end(); ++it) if (it->first == id) { listeners_.erase(it); break; }
    }

    // ---- Auth flows (same rules as the Dart app) ----
    static std::string signupUsername(const std::string& username);  // "" = let the DB pick
    static std::string normalizeEmailOtpDigits(const std::string& v);
    static bool isSixDigitEmailOtp(const std::string& v);
    static constexpr const char* internalDomain = "users.nexora.invalid";
    static constexpr const char* emailRedirect = "nexora://login-callback";
    static bool isUsernameLogin(const std::string& id);
    static std::string loginEmail(const std::string& id);
    static std::string loginPassword(const std::string& id, const std::string& password);

    int otpCooldownLeft(const std::string& email) const;
    void sendEmailOtp(const std::string& email, std::function<void(bool)> cb);
    void verifyEmailOtp(const std::string& email, const std::string& token, std::function<void(bool)> cb);
    void registerEmail(const std::string& email, const std::string& password, const std::string& username,
                       std::function<void(bool)> cb);
    void signInEmail(const std::string& idOrEmail, const std::string& password, std::function<void(bool)> cb);
    void changePassword(const std::string& newPassword, std::function<void(bool)> cb);
    void resetPassword(const std::string& email, std::function<void(bool)> cb);
    /// Handles nexora://login-callback#access_token=..&refresh_token=..&type=recovery|magiclink|signup.
    /// Returns the link type ("" when the link carries no session). [cb] fires after the session is set.
    std::string handleAuthDeepLink(const std::string& url, std::function<void(bool ok, const std::string& type)> cb);
    static std::map<std::string, std::string> parseLinkParams(const std::string& url);
    void signOut(std::function<void()> cb = nullptr);

    /// Registers this player on the games project (profile mirror row). Needed before every games RPC.
    void gamesHello(bool force, std::function<void(bool)> cb = nullptr);
    /// RPC on the games project (calls gamesHello first). Fails with "offline" without a session.
    void grpc(const std::string& fn, const Json& params, SbCallback cb);
    /// RPC on the main project without a session (anon role), e.g. username_available before sign-up.
    void rpcAnon(const std::string& fn, const Json& params, SbCallback cb) {
        if (!main_) { SbResult r; r.error = "not_configured"; if (cb) cb(r); return; }
        main_->rpc(fn, params, cb);
    }
    /// RPC on the main project.
    void rpc(const std::string& fn, const Json& params, SbCallback cb);

    /// Drives realtime heartbeats / reconnects. Call from the scene scheduler.
    void tick(int64_t nowMs);

private:
    void runAuth(std::function<void(SbCallback)> op, std::function<void(bool)> cb);
    void notify();
    void onAuth(AuthEvent ev, const AuthSession& s);

    OnlineConfig cfg_;
    Clock clock_;
    std::unique_ptr<SupabaseClient> main_, games_;
    std::unique_ptr<RealtimeClient> rtMain_, rtGames_;
    BackendStatus status_ = BackendStatus::Offline;
    bool busy_ = false;
    std::string lastError_;
    int lastRetryAfter_ = -1;
    std::string helloFor_;
    std::map<std::string, int64_t> otpSentAt_;
    std::vector<std::pair<int, std::function<void()>>> listeners_;
    int nextListener_ = 0;
};

}  // namespace nx
