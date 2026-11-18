#include "nxtest.h"

#include "backend/MatchLink.h"
#include "backend/OnlineService.h"
#include "backend/Realtime.h"
#include "backend/SupabaseClient.h"

#include <deque>
#include <fstream>
#include <sstream>

using namespace nx;

namespace {

struct FakeHttp : IHttp {
    struct Pending { HttpRequest req; HttpCallback cb; };
    std::deque<Pending> pending;
    std::vector<HttpRequest> log;
    void send(const HttpRequest& req, HttpCallback cb) override {
        log.push_back(req);
        pending.push_back({req, std::move(cb)});
    }
    HttpRequest respond(long status, const std::string& body) {
        Pending p = std::move(pending.front());
        pending.pop_front();
        HttpResponse r;
        r.status = status;
        r.body = body;
        p.cb(r);
        return p.req;
    }
};

struct FakeWs : IWebSocket {
    static std::vector<std::string>* sent;
    static std::string* lastUrl;
    bool openFlag = false;
    Listener l;
    bool open(const std::string& url, Listener listener) override { *lastUrl = url; l = listener; return true; }
    void sendText(const std::string& t) override { sent->push_back(t); }
    void close() override { openFlag = false; }
    bool isOpen() const override { return openFlag; }
};
std::vector<std::string>* FakeWs::sent = nullptr;
std::string* FakeWs::lastUrl = nullptr;

int64_t gNow = 1700000000000;

std::string readGoldenFile(const std::string& name) {
    std::ifstream f(std::string(NX_GOLDEN_DIR) + "/" + name);
    std::stringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

}  // namespace

TEST(postgrest_query_building) {
    FakeHttp http;
    MemoryStore st;
    SupabaseClient c("https://abc.supabase.co/", "anon", &http, &st, "k", [] { return gNow; });
    HttpRequest r = c.from("nx_match_moves").select("seq, seat, kind").eq("match_id", "m 1").gt("seq", 3).order("seq").build("GET");
    CHECK_EQ(r.url, std::string("https://abc.supabase.co/rest/v1/nx_match_moves?select=seq%2Cseat%2Ckind&match_id=eq.m%201&seq=gt.3&order=seq.asc"));
    CHECK_EQ(r.header("apikey"), std::string("anon"));
    CHECK_EQ(r.header("Authorization"), std::string("Bearer anon"));
    HttpRequest in = c.from("profiles").select("id").in("id", {"a", "b c"}).limit(5).build("GET");
    CHECK_EQ(in.url, std::string("https://abc.supabase.co/rest/v1/profiles?select=id&id=in.%28a%2C%22b%20c%22%29&limit=5"));
}

TEST(auth_password_signin_persists_and_refreshes) {
    FakeHttp http;
    MemoryStore st;
    SupabaseClient c("https://abc.supabase.co", "anon", &http, &st, "sess", [] { return gNow; });
    int events = 0;
    c.onAuthStateChange([&](AuthEvent, const AuthSession&) { ++events; });
    bool done = false;
    c.signInWithPassword("a@b.c", "pw", [&](const SbResult& r) { done = r.ok; });
    HttpRequest req = http.respond(200, R"({"access_token":"AT1","refresh_token":"RT1","expires_in":3600,"user":{"id":"u1","email":"a@b.c"}})");
    CHECK_EQ(req.url, std::string("https://abc.supabase.co/auth/v1/token?grant_type=password"));
    CHECK_EQ(req.body, std::string(R"({"email":"a@b.c","password":"pw"})"));
    CHECK(done);
    CHECK_EQ(c.session().userId, std::string("u1"));
    CHECK_EQ(events, 1);
    CHECK(!st.getString("sess").empty());

    // Restore in a new client.
    SupabaseClient c2("https://abc.supabase.co", "anon", &http, &st, "sess", [] { return gNow; });
    CHECK(c2.restoreSession());
    CHECK_EQ(c2.session().accessToken, std::string("AT1"));

    // Expired token -> rpc triggers refresh first, then the rpc carries the new token.
    gNow += 3600 * 1000;
    bool rpcOk = false;
    c2.rpc("nx_award", Json::obj({{"p_event", "x"}}), [&](const SbResult& r) { rpcOk = r.ok; });
    CHECK_EQ(http.pending.size(), (size_t)1);
    HttpRequest ref = http.respond(200, R"({"access_token":"AT2","refresh_token":"RT2","expires_in":3600,"user":{"id":"u1"}})");
    CHECK_EQ(ref.url, std::string("https://abc.supabase.co/auth/v1/token?grant_type=refresh_token"));
    CHECK_EQ(ref.body, std::string(R"({"refresh_token":"RT1"})"));
    HttpRequest rpc = http.respond(200, "null");
    CHECK_EQ(rpc.url, std::string("https://abc.supabase.co/rest/v1/rpc/nx_award"));
    CHECK_EQ(rpc.header("Authorization"), std::string("Bearer AT2"));
    CHECK(rpcOk);
}

TEST(auth_error_normalization) {
    SbResult r;
    r.status = 429;
    r.error = "For security purposes, you can only request this after 42 seconds.";
    SupabaseClient::normalizeAuthError(r);
    CHECK_EQ(r.error, std::string("rate_limit"));
    CHECK_EQ(r.retryAfter, 42);
    SbResult b;
    b.status = 400;
    b.error = "Invalid login credentials";
    b.errorCode = "invalid_credentials";
    SupabaseClient::normalizeAuthError(b);
    CHECK_EQ(b.error, std::string("invalid_login"));
    HttpResponse h;
    h.status = 400;
    h.body = R"({"code":400,"error_code":"otp_expired","msg":"Token has expired or is invalid"})";
    SbResult c = SupabaseClient::toResult(h);
    SupabaseClient::normalizeAuthError(c);
    CHECK_EQ(c.error, std::string("otp_invalid"));
}

TEST(online_service_login_rules) {
    CHECK_EQ(OnlineService::loginEmail(" Admin "), std::string("admin@users.nexora.invalid"));
    CHECK_EQ(OnlineService::loginPassword("admin", "12"), std::string("nx#12"));
    CHECK_EQ(OnlineService::loginPassword("a@b.c", "12"), std::string("12"));
    CHECK_EQ(OnlineService::signupUsername("ab"), std::string(""));
    CHECK_EQ(OnlineService::signupUsername(" علی‌رضا "), std::string("علی‌رضا"));
    CHECK_EQ(OnlineService::normalizeEmailOtpDigits("۱۲۳ ۴۵۶"), std::string("123456"));
    CHECK(OnlineService::isSixDigitEmailOtp("123456"));
    CHECK(!OnlineService::isSixDigitEmailOtp("12345a"));
}

TEST(online_service_games_client_uses_main_token) {
    FakeHttp http;
    MemoryStore st;
    std::vector<std::string> sent;
    std::string url;
    FakeWs::sent = &sent;
    FakeWs::lastUrl = &url;
    OnlineConfig cfg{"https://main.supabase.co", "mainkey", "https://games.supabase.co", "gameskey", ""};
    OnlineService os(cfg, &http, &st, [] { return std::unique_ptr<IWebSocket>(new FakeWs()); }, [] { return gNow; });
    bool ok = false;
    os.signInEmail("pilot@x.io", "secret", [&](bool b) { ok = b; });
    http.respond(200, R"({"access_token":"TOK","refresh_token":"R","expires_in":3600,"user":{"id":"u9","email":"pilot@x.io"}})");
    CHECK(ok);
    CHECK(os.isOnline());
    // Sign-in triggers nx_g_hello on the games project with the main project's JWT.
    CHECK_EQ(http.pending.size(), (size_t)1);
    HttpRequest hello = http.respond(200, "null");
    CHECK_EQ(hello.url, std::string("https://games.supabase.co/rest/v1/rpc/nx_g_hello"));
    CHECK_EQ(hello.header("apikey"), std::string("gameskey"));
    CHECK_EQ(hello.header("Authorization"), std::string("Bearer TOK"));
    SbResult got;
    os.grpc("nx_quick", Json::obj({{"p_game", "ludo"}}), [&](const SbResult& r) { got = r; });
    HttpRequest q = http.respond(200, R"({"room":"r1"})");
    CHECK_EQ(q.url, std::string("https://games.supabase.co/rest/v1/rpc/nx_quick"));
    CHECK_EQ(got.data["room"].asString(), std::string("r1"));
}

TEST(realtime_phoenix_join_presence_broadcast) {
    std::vector<std::string> sent;
    std::string url;
    FakeWs::sent = &sent;
    FakeWs::lastUrl = &url;
    FakeWs* ws = nullptr;
    RealtimeClient rt("https://g.supabase.co", "key", [&] {
        auto p = std::unique_ptr<IWebSocket>(new FakeWs());
        ws = static_cast<FakeWs*>(p.get());
        return p;
    }, [] { return gNow; });
    rt.setAuth("JWT");
    auto ch = rt.channel("nxm:m1", "0");
    int rows = 0, lives = 0, syncs = 0;
    ch->onPostgresChanges({"INSERT", "public", "nx_match_moves", "match_id=eq.m1"},
                          [&](const Json& rec, const Json&, const std::string& t) { rows += rec["seq"].asInt() == 1 && t == "INSERT"; })
        .onBroadcast("live", [&](const Json& p) { lives += p["payload"]["e"].asString() == "aim"; })
        .onPresenceSync([&] { ++syncs; });
    bool subscribed = false;
    ch->subscribe([&](RealtimeChannel::Status s, const std::string&) { subscribed = s == RealtimeChannel::Status::Subscribed; });
    CHECK_EQ(url, std::string("wss://g.supabase.co/realtime/v1/websocket?apikey=key&vsn=1.0.0"));
    CHECK(sent.empty());  // buffered until open
    ws->openFlag = true;
    rt.injectOpen();
    CHECK_EQ(sent.size(), (size_t)1);
    PhxMessage join;
    CHECK(PhxMessage::decode(sent[0], join));
    CHECK_EQ(join.topic, std::string("realtime:nxm:m1"));
    CHECK_EQ(join.event, std::string("phx_join"));
    CHECK_EQ(join.payload["access_token"].asString(), std::string("JWT"));
    CHECK_EQ(join.payload["config"]["presence"]["key"].asString(), std::string("0"));
    CHECK_EQ(join.payload["config"]["postgres_changes"][0]["filter"].asString(), std::string("match_id=eq.m1"));
    rt.injectMessage(R"({"topic":"realtime:nxm:m1","event":"phx_reply","ref":")" + join.ref +
                     R"(","join_ref":")" + join.ref + R"(","payload":{"status":"ok","response":{"postgres_changes":[{"id":77}]}}})");
    CHECK(subscribed);
    ch->track(Json::obj({{"seat", 0}}));
    PhxMessage tr;
    CHECK(PhxMessage::decode(sent.back(), tr));
    CHECK_EQ(tr.payload["event"].asString(), std::string("track"));
    rt.injectMessage(R"({"topic":"realtime:nxm:m1","event":"postgres_changes","ref":null,"payload":{"ids":[77],"data":{"type":"INSERT","table":"nx_match_moves","record":{"seq":1}}}})");
    CHECK_EQ(rows, 1);
    rt.injectMessage(R"({"topic":"realtime:nxm:m1","event":"postgres_changes","ref":null,"payload":{"ids":[5],"data":{"type":"INSERT","table":"x","record":{"seq":1}}}})");
    CHECK_EQ(rows, 1);  // other binding id ignored
    rt.injectMessage(R"({"topic":"realtime:nxm:m1","event":"broadcast","payload":{"type":"broadcast","event":"live","payload":{"s":1,"e":"aim","d":{}}}})");
    CHECK_EQ(lives, 1);
    rt.injectMessage(R"({"topic":"realtime:nxm:m1","event":"presence_state","payload":{"0":{"metas":[{"phx_ref":"a","seat":0}]},"1":{"metas":[{"phx_ref":"b","seat":1}]}}})");
    CHECK_EQ(ch->presenceState().size(), (size_t)2);
    rt.injectMessage(R"({"topic":"realtime:nxm:m1","event":"presence_diff","payload":{"joins":{},"leaves":{"1":{"metas":[{"phx_ref":"b","seat":1}]}}}})");
    CHECK_EQ(ch->presenceState().size(), (size_t)1);
    CHECK_EQ(syncs, 2);
    // Heartbeat after 25 s.
    size_t before = sent.size();
    gNow += 26000;
    rt.tick(gNow);
    CHECK_EQ(sent.size(), before + 1);
    PhxMessage hb;
    PhxMessage::decode(sent.back(), hb);
    CHECK_EQ(hb.topic, std::string("phoenix"));
    CHECK_EQ(hb.event, std::string("heartbeat"));
    // Drop + reconnect rejoins with a fresh ref.
    rt.injectClose();
    CHECK(ch->state() == RealtimeChannel::State::Errored);
    gNow += 1500;
    rt.tick(gNow);
    ws->openFlag = true;
    rt.injectOpen();
    PhxMessage rejoin;
    PhxMessage::decode(sent.back(), rejoin);
    CHECK_EQ(rejoin.event, std::string("phx_join"));
    CHECK(rejoin.ref != join.ref);
}

TEST(match_link_ordering_and_resync) {
    FakeHttp http;
    MemoryStore st;
    SupabaseClient games("https://g.supabase.co", "gk", &http, nullptr, "", [] { return gNow; });
    games.setAccessTokenProvider([] { return std::string("TOK"); });
    Json row = Json::parse(R"({"id":"m1","players":["uA","uB"],"names":["Ali"],"seed":99,"game":"ludo","status":"playing","settings":{}})");
    SupabaseMatchLink link(&games, nullptr, "m1", "uB", row);
    CHECK_EQ(link.mySeat(), 1);
    CHECK_EQ(link.names()[1], std::string("Pilot 2"));
    CHECK_EQ(link.seed(), (int64_t)99);
    std::vector<int> got;
    bool replayDone = false;
    link.listen([&](const NetMove& m) { got.push_back(m.seq * 10 + (m.replay ? 1 : 0)); }, [&] { replayDone = true; });
    // catch-up returns two rows out of order.
    HttpRequest cu = http.respond(200, R"([{"seq":2,"seat":1,"kind":"move","move":{"t":2},"hash":null,"by_user":"uB"},{"seq":1,"seat":0,"kind":"move","move":{"t":1},"hash":"ab","by_user":"uA"}])");
    CHECK(cu.url.find("nx_match_moves?select=seq%2Cseat%2Ckind%2Cmove%2Chash%2Cby_user&match_id=eq.m1&seq=gt.0&order=seq.asc") != std::string::npos);
    http.respond(200, R"([{"status":"playing"}])");
    CHECK(replayDone);
    CHECK_EQ(got.size(), (size_t)2);
    CHECK_EQ(got[0], 11);
    CHECK_EQ(got[1], 21);
    // Live row 3 is delivered; duplicate ignored; gap (5) waits for 4.
    link.onRow(Json::parse(R"({"seq":3,"seat":0,"kind":"move","move":{},"by_user":"uA"})"));
    link.onRow(Json::parse(R"({"seq":3,"seat":0,"kind":"move","move":{},"by_user":"uA"})"));
    link.onRow(Json::parse(R"({"seq":5,"seat":0,"kind":"move","move":{},"by_user":"uA"})"));
    CHECK_EQ(got.size(), (size_t)3);
    link.onRow(Json::parse(R"({"seq":4,"seat":0,"kind":"forfeit","move":{"seat":0},"by_user":"uA"})"));
    CHECK_EQ(got.size(), (size_t)5);
    CHECK(link.forfeitedSeats().count(0) == 1);
    CHECK(link.isAuthority());  // seat 0 forfeited -> seat 1 is authority
    // Our send takes seq 6 optimistically; the server says someone else took it -> resync.
    bool sendOk = true;
    link.send(Json::obj({{"x", 1}}), "move", "h", -1, [&](bool ok) { sendOk = ok; });
    HttpRequest push = http.respond(200, R"({"ok":false,"reason":"seq"})");
    CHECK_EQ(push.body, std::string(R"({"p_match":"m1","p_seq":6,"p_kind":"move","p_move":{"x":1},"p_hash":"h","p_seat":1})"));
    http.respond(200, R"([{"seq":6,"seat":0,"kind":"timeout","move":{},"by_user":"uA"}])");
    http.respond(200, R"([{"status":"playing"}])");
    CHECK(!sendOk);
    CHECK(link.resyncRequests() >= 1);  // Dart also fires twice here (row handler + send handler)
    // Match end via realtime update.
    link.onMatch(Json::parse(R"({"status":"finished","winner_seats":[1],"reason":"forfeit:0"})"));
    CHECK(link.isOver());
    CHECK_EQ(link.end().forfeitSeat(), 0);
}

TEST(hash_of_matches_dart) {
    // Values produced by lib/games/core/net_link.dart hashOf() (see tests/golden/hash.txt).
    const char* inputs[] = {"", "abc", "سلام دنیا", "{\"a\":1,\"b\":[2,3]}", "😀x"};
    std::string out;
    for (auto s : inputs) out += hashOf(s) + "\n";
    CHECK_EQ(out, readGoldenFile("hash.txt"));
}
