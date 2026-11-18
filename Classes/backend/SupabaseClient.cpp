#include "backend/SupabaseClient.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <regex>

namespace nx {

int64_t systemNowMs() {
    using namespace std::chrono;
    return duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
}

std::string urlEncode(const std::string& s) {
    static const char* hex = "0123456789ABCDEF";
    std::string o;
    for (unsigned char c : s) {
        if (std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') o.push_back(static_cast<char>(c));
        else {
            o.push_back('%');
            o.push_back(hex[c >> 4]);
            o.push_back(hex[c & 15]);
        }
    }
    return o;
}

// ---------------- AuthSession ----------------

Json AuthSession::toJson() const {
    return Json::obj({{"access_token", accessToken},
                      {"refresh_token", refreshToken},
                      {"expires_at", expiresAt},
                      {"user", user.isNull() ? Json::obj({{"id", userId}, {"email", email}, {"is_anonymous", isAnonymous}}) : user}});
}

static void fillUser(AuthSession& s, const Json& u) {
    s.user = u;
    s.userId = u["id"].asString();
    s.email = u["email"].asString();
    s.isAnonymous = u["is_anonymous"].asBool(false);
}

AuthSession AuthSession::fromJson(const Json& j) {
    AuthSession s;
    s.accessToken = j["access_token"].asString();
    s.refreshToken = j["refresh_token"].asString();
    s.expiresAt = j["expires_at"].asInt(0);
    fillUser(s, j["user"]);
    return s;
}

AuthSession AuthSession::fromTokenResponse(const Json& j, int64_t nowSec) {
    AuthSession s = fromJson(j);
    if (s.expiresAt == 0) s.expiresAt = nowSec + j["expires_in"].asInt(3600);
    return s;
}

// ---------------- Query ----------------

static std::string filterValue(const Json& v) {
    if (v.isString()) return v.asString();
    if (v.isBool()) return v.asBool() ? "true" : "false";
    if (v.isNull()) return "null";
    if (v.isInt()) return std::to_string(v.asInt());
    return v.str();
}

Query& Query::filter(const std::string& col, const std::string& op, const Json& v) {
    params_.emplace_back(col, op + "." + filterValue(v));
    return *this;
}

Query& Query::in(const std::string& col, const std::vector<std::string>& vals) {
    std::string s = "in.(";
    for (size_t i = 0; i < vals.size(); ++i) {
        if (i) s += ",";
        // PostgREST: quote values containing reserved chars.
        bool quote = vals[i].find_first_of(",()\" ") != std::string::npos;
        s += quote ? "\"" + vals[i] + "\"" : vals[i];
    }
    s += ")";
    params_.emplace_back(col, s);
    return *this;
}

std::string Query::queryString() const {
    std::vector<std::pair<std::string, std::string>> q;
    if (!select_.empty()) {
        std::string compact;
        for (char ch : select_) if (ch != ' ') compact.push_back(ch);  // supabase-dart strips whitespace
        q.emplace_back("select", compact);
    }
    for (auto& p : params_) q.push_back(p);
    if (!order_.empty()) {
        std::string o;
        for (size_t i = 0; i < order_.size(); ++i) o += (i ? "," : "") + order_[i];
        q.emplace_back("order", o);
    }
    if (limit_ >= 0) q.emplace_back("limit", std::to_string(limit_));
    if (rangeFrom_ >= 0) {
        q.emplace_back("offset", std::to_string(rangeFrom_));
        q.emplace_back("limit", std::to_string(rangeTo_ - rangeFrom_ + 1));
    }
    std::string s;
    for (size_t i = 0; i < q.size(); ++i) {
        s += (i ? "&" : "?") + urlEncode(q[i].first) + "=" + urlEncode(q[i].second);
    }
    return s;
}

HttpRequest Query::build(const std::string& method, const std::string& body) const {
    HttpRequest r;
    r.method = method;
    r.url = c_->url() + "/rest/v1/" + table_ + queryString();
    r.headers = c_->baseHeaders(true);
    for (auto& h : extraHeaders_) r.headers.push_back(h);
    r.body = body;
    r.tag = method + " " + table_;
    return r;
}

void Query::get(SbCallback cb) { c_->execute(build("GET"), std::move(cb)); }

void Query::single(SbCallback cb) {
    HttpRequest r = build("GET");
    r.headers.emplace_back("Accept", "application/vnd.pgrst.object+json");
    c_->execute(r, std::move(cb), true);
}

void Query::maybeSingle(SbCallback cb) {
    Query copy = *this;
    if (copy.limit_ < 0) copy.limit_ = 2;
    c_->execute(copy.build("GET"), std::move(cb), false, true);
}

void Query::insert(const Json& rows, SbCallback cb, bool returning) {
    HttpRequest r = build("POST", rows.dump());
    r.headers.emplace_back("Prefer", returning ? "return=representation" : "return=minimal");
    c_->execute(r, std::move(cb));
}

void Query::upsert(const Json& rows, SbCallback cb, const std::string& onConflict) {
    Query copy = *this;
    if (!onConflict.empty()) copy.params_.emplace_back("on_conflict", onConflict);
    HttpRequest r = copy.build("POST", rows.dump());
    r.headers.emplace_back("Prefer", "resolution=merge-duplicates,return=representation");
    c_->execute(r, std::move(cb));
}

void Query::update(const Json& patch, SbCallback cb) {
    HttpRequest r = build("PATCH", patch.dump());
    r.headers.emplace_back("Prefer", "return=representation");
    c_->execute(r, std::move(cb));
}

void Query::remove(SbCallback cb) {
    HttpRequest r = build("DELETE");
    r.headers.emplace_back("Prefer", "return=representation");
    c_->execute(r, std::move(cb));
}

// ---------------- SupabaseClient ----------------

SupabaseClient::SupabaseClient(std::string url, std::string anonKey, IHttp* http, IKeyValueStore* store,
                               std::string storageKey, Clock clock)
    : url_(std::move(url)), anonKey_(std::move(anonKey)), http_(http), store_(store), storageKey_(std::move(storageKey)),
      clock_(clock ? std::move(clock) : Clock(systemNowMs)) {
    while (!url_.empty() && url_.back() == '/') url_.pop_back();
}

std::string SupabaseClient::accessToken() const {
    if (tokenProvider_) {
        std::string t = tokenProvider_();
        if (!t.empty()) return t;
    }
    return session_.valid() ? session_.accessToken : "";
}

std::vector<std::pair<std::string, std::string>> SupabaseClient::baseHeaders(bool json) const {
    std::vector<std::pair<std::string, std::string>> h;
    h.emplace_back("apikey", anonKey_);
    std::string tok = accessToken();
    h.emplace_back("Authorization", "Bearer " + (tok.empty() ? anonKey_ : tok));
    if (json) h.emplace_back("Content-Type", "application/json");
    h.emplace_back("X-Client-Info", "nexora-cocos/1.0");
    return h;
}

SbResult SupabaseClient::toResult(const HttpResponse& r) {
    SbResult out;
    out.status = r.status;
    if (r.status == 0) {
        out.error = "network";
        return out;
    }
    bool parsed = false;
    Json body = r.body.empty() ? Json() : Json::parse(r.body, &parsed);
    if (r.ok()) {
        out.ok = true;
        out.data = r.body.empty() ? Json() : (parsed ? body : Json(r.body));
        return out;
    }
    // Error shapes: GoTrue {error_code, msg} / {error, error_description} ; PostgREST {code, message, details, hint}.
    std::string msg;
    if (parsed) {
        for (const char* k : {"msg", "message", "error_description", "error"}) {
            if (body[k].isString() && !body[k].asString().empty()) { msg = body[k].asString(); break; }
        }
        out.errorCode = body["error_code"].isString() ? body["error_code"].asString() : body["code"].str();
        if (body["code"].isNull() && body["error_code"].isNull()) out.errorCode.clear();
        out.data = body;
    }
    if (msg.empty()) msg = r.body.empty() ? ("HTTP " + std::to_string(r.status)) : r.body;
    out.error = msg;
    return out;
}

void SupabaseClient::normalizeAuthError(SbResult& r) {
    if (r.ok) return;
    if (r.status == 0) { r.error = "network"; return; }
    std::string m = r.error;
    std::transform(m.begin(), m.end(), m.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    std::string code = r.errorCode;
    std::transform(code.begin(), code.end(), code.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    auto has = [&](const char* s) { return m.find(s) != std::string::npos; };
    if (r.status == 429 || code.find("rate_limit") != std::string::npos || has("rate limit") || has("security purposes") ||
        has("too many")) {
        std::smatch sm;
        if (std::regex_search(m, sm, std::regex("after (\\d+) second"))) r.retryAfter = std::stoi(sm[1].str());
        r.error = "rate_limit";
        return;
    }
    if (code == "otp_expired" || has("token has expired") || (has("otp") && has("invalid"))) { r.error = "otp_invalid"; return; }
    if (code == "email_address_not_authorized" || has("not authorized") || has("error sending") || has("smtp")) {
        r.error = "smtp";
        return;
    }
    if (code == "signup_disabled" || has("signups not allowed")) { r.error = "signup_disabled"; return; }
    if (code == "invalid_credentials" || has("invalid login")) { r.error = "invalid_login"; return; }
    if (has("already") || has("registered")) { r.error = "email_taken"; return; }
    if (has("password")) { r.error = "weak_password"; return; }
}

void SupabaseClient::execute(HttpRequest req, SbCallback cb, bool singleObject, bool maybeSingle) {
    auto run = [this, req, cb, singleObject, maybeSingle](bool) mutable {
        // Re-stamp auth headers: the token may have just been refreshed.
        auto fresh = baseHeaders(req.header("Content-Type") == "application/json");
        for (auto& f : fresh) {
            for (auto& h : req.headers) if (h.first == f.first) h.second = f.second;
        }
        http_->send(req, [cb, singleObject, maybeSingle](const HttpResponse& r) {
            SbResult res = toResult(r);
            if (res.ok && maybeSingle) {
                if (res.data.isArray()) {
                    if (res.data.size() > 1) {
                        res.ok = false;
                        res.error = "multiple_rows";
                    }
                    res.data = res.data.size() == 1 ? res.data[0] : Json();
                }
            }
            (void)singleObject;
            if (cb) cb(res);
        });
    };
    if (session_.valid() && !tokenProvider_) ensureFreshToken(run);
    else run(true);
}

void SupabaseClient::rpc(const std::string& fn, const Json& params, SbCallback cb) {
    HttpRequest r;
    r.method = "POST";
    r.url = url_ + "/rest/v1/rpc/" + fn;
    r.headers = baseHeaders(true);
    r.body = params.isNull() ? "{}" : params.dump();
    r.tag = "rpc " + fn;
    execute(r, std::move(cb));
}

void SupabaseClient::invokeFunction(const std::string& name, const Json& body, SbCallback cb) {
    HttpRequest r;
    r.method = "POST";
    r.url = url_ + "/functions/v1/" + name;
    r.headers = baseHeaders(true);
    r.body = body.isNull() ? "{}" : body.dump();
    r.tag = "fn " + name;
    execute(r, std::move(cb));
}

void SupabaseClient::storageUpload(const std::string& bucket, const std::string& path, const std::string& bytes,
                                   const std::string& contentType, bool upsert, SbCallback cb) {
    HttpRequest r;
    r.method = "POST";
    r.url = url_ + "/storage/v1/object/" + bucket + "/" + path;
    r.headers = baseHeaders(false);
    r.headers.emplace_back("Content-Type", contentType);
    r.headers.emplace_back("x-upsert", upsert ? "true" : "false");
    r.headers.emplace_back("cache-control", "max-age=3600");
    r.body = bytes;
    r.tag = "upload " + bucket;
    execute(r, std::move(cb));
}

std::string SupabaseClient::storagePublicUrl(const std::string& bucket, const std::string& path) const {
    return url_ + "/storage/v1/object/public/" + bucket + "/" + path;
}

// ---- auth ----

bool SupabaseClient::restoreSession() {
    if (!store_) return false;
    std::string raw = store_->getString(storageKey_, "");
    if (raw.empty()) return false;
    bool ok = false;
    Json j = Json::parse(raw, &ok);
    if (!ok) return false;
    AuthSession s = AuthSession::fromJson(j);
    if (!s.valid() || s.refreshToken.empty()) return false;
    session_ = s;
    emit(AuthEvent::InitialSession);
    return true;
}

void SupabaseClient::persist() {
    if (!store_) return;
    if (session_.valid()) store_->setString(storageKey_, session_.toJson().dump());
    else store_->remove(storageKey_);
}

void SupabaseClient::emit(AuthEvent ev) {
    auto ls = listeners_;
    for (auto& l : ls) l(ev, session_);
}

void SupabaseClient::setSession(const AuthSession& s, AuthEvent ev) {
    session_ = s;
    persist();
    emit(ev);
}

void SupabaseClient::clearSession() {
    session_ = AuthSession();
    persist();
    emit(AuthEvent::SignedOut);
}

void SupabaseClient::authPost(const std::string& path, const Json& body, bool expectSession, AuthEvent ev, SbCallback cb,
                              const std::string& method) {
    HttpRequest r;
    r.method = method;
    r.url = url_ + "/auth/v1/" + path;
    r.headers = baseHeaders(true);
    r.body = body.dump();
    r.tag = "auth " + path;
    http_->send(r, [this, expectSession, ev, cb](const HttpResponse& resp) {
        SbResult res = toResult(resp);
        if (!res.ok) {
            normalizeAuthError(res);
        } else if (expectSession && res.data.has("access_token")) {
            setSession(AuthSession::fromTokenResponse(res.data, nowSec()), ev);
        } else if (ev == AuthEvent::UserUpdated && res.data.has("id")) {
            AuthSession s = session_;
            fillUser(s, res.data);
            setSession(s, ev);
        }
        if (cb) cb(res);
    });
}

void SupabaseClient::signInWithPassword(const std::string& email, const std::string& password, SbCallback cb) {
    authPost("token?grant_type=password", Json::obj({{"email", email}, {"password", password}}), true, AuthEvent::SignedIn,
             std::move(cb));
}

void SupabaseClient::signUp(const std::string& email, const std::string& password, const Json& data, SbCallback cb) {
    Json b = Json::obj({{"email", email}, {"password", password}});
    if (!data.isNull()) b.set("data", data);
    authPost("signup", b, true, AuthEvent::SignedIn, std::move(cb));
}

void SupabaseClient::signInWithOtp(const std::string& email, bool createUser, const std::string& redirectTo, SbCallback cb) {
    std::string path = "otp";
    if (!redirectTo.empty()) path += "?redirect_to=" + urlEncode(redirectTo);
    authPost(path, Json::obj({{"email", email}, {"create_user", createUser}}), false, AuthEvent::SignedIn, std::move(cb));
}

void SupabaseClient::verifyOtp(const std::string& email, const std::string& token, const std::string& type, SbCallback cb) {
    authPost("verify", Json::obj({{"type", type}, {"email", email}, {"token", token}}), true, AuthEvent::SignedIn, std::move(cb));
}

void SupabaseClient::refreshSession(SbCallback cb) {
    if (session_.refreshToken.empty()) {
        SbResult r;
        r.error = "no_session";
        if (cb) cb(r);
        return;
    }
    HttpRequest r;
    r.method = "POST";
    r.url = url_ + "/auth/v1/token?grant_type=refresh_token";
    r.headers = baseHeaders(true);
    for (auto& h : r.headers) if (h.first == "Authorization") h.second = "Bearer " + anonKey_;
    r.body = Json::obj({{"refresh_token", session_.refreshToken}}).dump();
    r.tag = "auth refresh";
    http_->send(r, [this, cb](const HttpResponse& resp) {
        SbResult res = toResult(resp);
        if (res.ok && res.data.has("access_token")) {
            setSession(AuthSession::fromTokenResponse(res.data, nowSec()), AuthEvent::TokenRefreshed);
        } else if (!res.ok && res.status >= 400 && res.status < 500) {
            // Refresh token revoked/expired: the session is gone (same as supabase-dart).
            clearSession();
        }
        if (cb) cb(res);
    });
}

void SupabaseClient::ensureFreshToken(std::function<void(bool)> then, int marginSec) {
    if (!session_.valid()) { then(false); return; }
    if (session_.expiresAt - nowSec() > marginSec) { then(true); return; }
    refreshWaiters_.push_back(std::move(then));
    if (refreshing_) return;
    refreshing_ = true;
    refreshSession([this](const SbResult& r) {
        refreshing_ = false;
        auto waiters = std::move(refreshWaiters_);
        refreshWaiters_.clear();
        for (auto& w : waiters) w(r.ok);
    });
}

void SupabaseClient::updateUser(const Json& attrs, SbCallback cb) {
    authPost("user", attrs, false, AuthEvent::UserUpdated, std::move(cb), "PUT");
}

void SupabaseClient::resetPasswordForEmail(const std::string& email, SbCallback cb, const std::string& redirectTo) {
    std::string path = "recover";
    if (!redirectTo.empty()) path += "?redirect_to=" + urlEncode(redirectTo);
    authPost(path, Json::obj({{"email", email}}), false, AuthEvent::SignedIn, std::move(cb));
}

void SupabaseClient::signInWithRefreshToken(const std::string& refreshToken, AuthEvent ev, SbCallback cb) {
    HttpRequest r;
    r.method = "POST";
    r.url = url_ + "/auth/v1/token?grant_type=refresh_token";
    r.headers = baseHeaders(true);
    for (auto& h : r.headers) if (h.first == "Authorization") h.second = "Bearer " + anonKey_;
    r.body = Json::obj({{"refresh_token", refreshToken}}).dump();
    r.tag = "auth refresh-signin";
    http_->send(r, [this, ev, cb](const HttpResponse& resp) {
        SbResult res = toResult(resp);
        if (res.ok && res.data.has("access_token")) setSession(AuthSession::fromTokenResponse(res.data, nowSec()), ev);
        else if (!res.ok) normalizeAuthError(res);
        if (cb) cb(res);
    });
}

void SupabaseClient::adoptTokenResponse(const Json& j) {
    if (j.has("access_token")) setSession(AuthSession::fromTokenResponse(j, nowSec()), AuthEvent::SignedIn);
}

void SupabaseClient::storageRemove(const std::string& bucket, const std::vector<std::string>& paths, SbCallback cb) {
    HttpRequest r;
    r.method = "DELETE";
    r.url = url_ + "/storage/v1/object/" + bucket;
    r.headers = baseHeaders(true);
    Json arr = Json::array();
    for (auto& p : paths) arr.push(Json(p));
    r.body = Json::obj({{"prefixes", arr}}).dump();
    r.tag = "storage remove " + bucket;
    execute(r, std::move(cb));
}

void SupabaseClient::signOut(SbCallback cb) {
    if (!session_.valid()) {
        clearSession();
        SbResult r;
        r.ok = true;
        if (cb) cb(r);
        return;
    }
    HttpRequest r;
    r.method = "POST";
    r.url = url_ + "/auth/v1/logout?scope=local";
    r.headers = baseHeaders(true);
    r.body = "{}";
    http_->send(r, [this, cb](const HttpResponse& resp) {
        SbResult res = toResult(resp);
        clearSession();  // local sign-out always succeeds
        res.ok = true;
        if (cb) cb(res);
    });
}

}  // namespace nx
