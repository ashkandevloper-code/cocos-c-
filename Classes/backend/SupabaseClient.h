// Supabase client over plain HTTP (GoTrue auth, PostgREST, RPC, Edge Functions, Storage).
// Mirrors what supabase_flutter does for the Dart app, with the same tables / RPC names / payloads.
#pragma once
#include "backend/Transport.h"
#include "core/Json.h"

#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace nx {

struct AuthSession {
    std::string accessToken;
    std::string refreshToken;
    int64_t expiresAt = 0;  // unix seconds
    std::string userId;
    std::string email;
    bool isAnonymous = false;
    Json user;
    bool valid() const { return !accessToken.empty() && !userId.empty(); }
    Json toJson() const;
    static AuthSession fromJson(const Json& j);
    /// Parses a GoTrue token/verify response ({access_token, refresh_token, expires_in|expires_at, user}).
    static AuthSession fromTokenResponse(const Json& j, int64_t nowSec);
};

/// Result of any call: data (parsed JSON) or a normalized error.
struct SbResult {
    bool ok = false;
    long status = 0;
    Json data;
    std::string error;       // normalized key (network, rate_limit, otp_invalid, smtp, ...) or server message
    std::string errorCode;   // raw "code"/"error_code" from the server
    int retryAfter = -1;     // seconds parsed from rate-limit messages
};
using SbCallback = std::function<void(const SbResult&)>;

enum class AuthEvent { SignedIn, SignedOut, TokenRefreshed, UserUpdated, InitialSession };

class SupabaseClient;

/// PostgREST query builder: client.from("nx_matches").select("*").eq("id", id).single(cb)
class Query {
public:
    Query(SupabaseClient* c, std::string table) : c_(c), table_(std::move(table)) {}
    Query& select(const std::string& cols = "*") { select_ = cols; return *this; }
    Query& eq(const std::string& col, const Json& v) { return filter(col, "eq", v); }
    Query& neq(const std::string& col, const Json& v) { return filter(col, "neq", v); }
    Query& gt(const std::string& col, const Json& v) { return filter(col, "gt", v); }
    Query& gte(const std::string& col, const Json& v) { return filter(col, "gte", v); }
    Query& lt(const std::string& col, const Json& v) { return filter(col, "lt", v); }
    Query& lte(const std::string& col, const Json& v) { return filter(col, "lte", v); }
    Query& is(const std::string& col, const std::string& v) { params_.emplace_back(col, "is." + v); return *this; }
    Query& in(const std::string& col, const std::vector<std::string>& vals);
    Query& ilike(const std::string& col, const std::string& pattern) { params_.emplace_back(col, "ilike." + pattern); return *this; }
    Query& orFilter(const std::string& expr) { params_.emplace_back("or", "(" + expr + ")"); return *this; }
    Query& order(const std::string& col, bool ascending = true) {
        order_.push_back(col + (ascending ? ".asc" : ".desc"));
        return *this;
    }
    Query& limit(int n) { limit_ = n; return *this; }
    Query& range(int from, int to) { rangeFrom_ = from; rangeTo_ = to; return *this; }

    void get(SbCallback cb);           // -> array
    void single(SbCallback cb);        // -> object, error if 0 or >1 rows
    void maybeSingle(SbCallback cb);   // -> object or null
    void insert(const Json& rows, SbCallback cb, bool returning = true);
    void upsert(const Json& rows, SbCallback cb, const std::string& onConflict = "");
    void update(const Json& patch, SbCallback cb);
    void remove(SbCallback cb);

    /// Built request (exposed for tests).
    HttpRequest build(const std::string& method, const std::string& body = "") const;

private:
    Query& filter(const std::string& col, const std::string& op, const Json& v);
    std::string queryString() const;
    SupabaseClient* c_;
    std::string table_;
    std::string select_;
    std::vector<std::pair<std::string, std::string>> params_;
    std::vector<std::string> order_;
    int limit_ = -1, rangeFrom_ = -1, rangeTo_ = -1;
    std::vector<std::pair<std::string, std::string>> extraHeaders_;
};

class SupabaseClient {
public:
    SupabaseClient(std::string url, std::string anonKey, IHttp* http, IKeyValueStore* store, std::string storageKey,
                   Clock clock = nullptr);

    const std::string& url() const { return url_; }
    const std::string& anonKey() const { return anonKey_; }
    bool configured() const { return !url_.empty() && !anonKey_.empty(); }

    // ---- Auth ----
    /// Restores the persisted session (no network). Returns true if one existed.
    bool restoreSession();
    const AuthSession& session() const { return session_; }
    bool hasSession() const { return session_.valid(); }
    /// A second client (games project) borrows the auth project's token instead of its own session.
    void setAccessTokenProvider(std::function<std::string()> p) { tokenProvider_ = std::move(p); }
    std::string accessToken() const;
    void onAuthStateChange(std::function<void(AuthEvent, const AuthSession&)> l) { listeners_.push_back(std::move(l)); }

    void signInWithPassword(const std::string& email, const std::string& password, SbCallback cb);
    void signUp(const std::string& email, const std::string& password, const Json& data, SbCallback cb);
    void signInWithOtp(const std::string& email, bool createUser, const std::string& redirectTo, SbCallback cb);
    void verifyOtp(const std::string& email, const std::string& token, const std::string& type, SbCallback cb);
    void refreshSession(SbCallback cb);
    void updateUser(const Json& attrs, SbCallback cb);
    void resetPasswordForEmail(const std::string& email, SbCallback cb);
    void signOut(SbCallback cb);
    /// Refreshes when the token expires within [marginSec]; calls [then] when the token is usable.
    void ensureFreshToken(std::function<void(bool ok)> then, int marginSec = 60);

    // ---- Data ----
    Query from(const std::string& table) { return Query(this, table); }
    void rpc(const std::string& fn, const Json& params, SbCallback cb);
    void invokeFunction(const std::string& name, const Json& body, SbCallback cb);
    void storageUpload(const std::string& bucket, const std::string& path, const std::string& bytes,
                       const std::string& contentType, bool upsert, SbCallback cb);
    std::string storagePublicUrl(const std::string& bucket, const std::string& path) const;

    // ---- Internals shared with Query / tests ----
    std::vector<std::pair<std::string, std::string>> baseHeaders(bool json = true) const;
    void execute(HttpRequest req, SbCallback cb, bool singleObject = false, bool maybeSingle = false);
    static SbResult toResult(const HttpResponse& r);
    /// Port of OnlineService._friendly: maps GoTrue errors to UI keys.
    static void normalizeAuthError(SbResult& r);
    int64_t nowSec() const { return clock_() / 1000; }

private:
    void setSession(const AuthSession& s, AuthEvent ev);
    void clearSession();
    void persist();
    void emit(AuthEvent ev);
    void authPost(const std::string& path, const Json& body, bool expectSession, AuthEvent ev, SbCallback cb,
                  const std::string& method = "POST");

    std::string url_, anonKey_;
    IHttp* http_;
    IKeyValueStore* store_;
    std::string storageKey_;
    Clock clock_;
    AuthSession session_;
    std::function<std::string()> tokenProvider_;
    std::vector<std::function<void(AuthEvent, const AuthSession&)>> listeners_;
    bool refreshing_ = false;
    std::vector<std::function<void(bool)>> refreshWaiters_;
};

}  // namespace nx
