// SocialService: RPC payloads, model parsing, optimistic follow, avatar upload flow, seen ticks, time handling.
#include "nxtest.h"

#include "backend/OnlineService.h"
#include "backend/SocialService.h"

#include <deque>

using namespace nx;

namespace {

struct SHttp : IHttp {
    struct P { HttpRequest req; HttpCallback cb; };
    std::deque<P> pending;
    void send(const HttpRequest& req, HttpCallback cb) override { pending.push_back({req, std::move(cb)}); }
    HttpRequest respond(long status, const std::string& body) {
        P p = std::move(pending.front());
        pending.pop_front();
        HttpResponse r;
        r.status = status;
        r.body = body;
        p.cb(r);
        return p.req;
    }
};
struct NoWs : IWebSocket {
    bool open(const std::string&, Listener) override { return true; }
    void sendText(const std::string&) override {}
    void close() override {}
    bool isOpen() const override { return false; }
};
int64_t sNow = 1795000000000;

struct Env {
    SHttp http;
    MemoryStore st;
    OnlineConfig cfg{"https://main.supabase.co", "anon", "", "", ""};
    OnlineService os{cfg, &http, &st, [] { return std::unique_ptr<IWebSocket>(new NoWs()); }, [] { return sNow; }};
    SocialService social{&os};
    Env() {
        os.signInEmail("a@x.io", "password1", [](bool) {});
        http.respond(200, R"({"access_token":"TOK","refresh_token":"R","expires_in":3600,"user":{"id":"me","email":"a@x.io"}})");
        while (!http.pending.empty()) http.respond(200, "null");
    }
};

const char* kProfileB = R"({"id":"b","username":"bob","display_name":"Bob","avatar_url":"https://x/b.jpg","bio":"hi",
 "website":"","location":"","is_private":false,"followers":3,"following":1,"posts":2,"is_me":false,"is_following":false,
 "follows_me":true,"is_pending":false,"is_blocked":false,"can_view":true,"pending_requests":0,"created_at":"2026-11-18T10:00:00+00:00"})";

}  // namespace

TEST(social_profile_rpc_and_parse) {
    Env e;
    SocialProfile got;
    e.social.profile("b", [&](const SbResult& r, const SocialProfile& p) { CHECK(r.ok); got = p; });
    HttpRequest q = e.http.respond(200, kProfileB);
    CHECK_EQ(q.url, std::string("https://main.supabase.co/rest/v1/rpc/nx_social_profile"));
    CHECK_EQ(q.header("Authorization"), std::string("Bearer TOK"));
    CHECK_EQ(Json::parse(q.body)["p_user"].asString(), std::string("b"));
    CHECK_EQ(got.user.id, std::string("b"));
    CHECK_EQ(got.user.username, std::string("bob"));
    CHECK_EQ(got.followers, 3);
    CHECK(got.followsMe);
    CHECK(got.followState() == FollowState::None);
}

TEST(social_optimistic_follow_public_private_and_rollback) {
    SocialProfile p = SocialProfile::fromJson(Json::parse(kProfileB));
    SocialProfile f = SocialService::optimisticFollow(p, true);
    CHECK(f.followState() == FollowState::Following);
    CHECK_EQ(f.followers, 4);
    CHECK_EQ(SocialService::optimisticFollow(f, true).followers, 4);  // no double count
    SocialProfile u = SocialService::optimisticFollow(f, false);
    CHECK(u.followState() == FollowState::None);
    CHECK_EQ(u.followers, 3);
    p.user.isPrivate = true;
    SocialProfile r = SocialService::optimisticFollow(p, true);
    CHECK(r.followState() == FollowState::Requested);
    CHECK_EQ(r.followers, 3);  // a request does not change counters
    CHECK(SocialService::optimisticFollow(r, false).followState() == FollowState::None);
    SocialProfile me = p;
    me.user.isMe = true;
    CHECK(SocialService::optimisticFollow(me, true).followState() == FollowState::Self);
}

TEST(social_follow_sends_toggle_and_surfaces_errors) {
    Env e;
    SbResult res;
    e.social.setFollow("b", true, [&](const SbResult& r, const SocialProfile&) { res = r; });
    HttpRequest q = e.http.respond(400, R"({"code":"P0001","message":"rate_limited"})");
    CHECK_EQ(q.url, std::string("https://main.supabase.co/rest/v1/rpc/nx_social_toggle_follow"));
    Json b = Json::parse(q.body);
    CHECK_EQ(b["p_target"].asString(), std::string("b"));
    CHECK(b["p_follow"].asBool());
    CHECK(!res.ok);
    CHECK_EQ(SocialService::friendlyError(res, false), std::string("Slow down a little and try again."));
}

TEST(social_avatar_upload_flow) {
    Env e;
    std::string jpeg("\xFF\xD8\xFF\xE0" "0123456789", 14);
    SbResult res;
    SocialProfile prof;
    e.social.uploadAvatar(jpeg, [&](const SbResult& r, const SocialProfile& p) { res = r; prof = p; });
    HttpRequest up = e.http.respond(200, R"({"Key":"avatars/me/x.jpg"})");
    CHECK(up.url.find("https://main.supabase.co/storage/v1/object/avatars/me/av_") == 0);
    CHECK(up.url.find(".jpg") != std::string::npos);
    CHECK_EQ(up.header("Content-Type"), std::string("image/jpeg"));
    CHECK_EQ(up.body, jpeg);
    std::string path = up.url.substr(std::string("https://main.supabase.co/storage/v1/object/avatars/").size());
    HttpRequest set = e.http.respond(200, std::string(R"({"old_path":"me/old.png","profile":{"id":"me","is_me":true,"avatar_url":"https://x/)") + path + "\"}}");
    CHECK_EQ(Json::parse(set.body)["p_path"].asString(), path);
    CHECK(res.ok);
    CHECK(prof.user.avatarUrl.find(path) != std::string::npos);
    HttpRequest del = e.http.respond(200, "[]");  // previous avatar file is deleted
    CHECK_EQ(del.method, std::string("DELETE"));
    CHECK(del.body.find("me/old.png") != std::string::npos);
}

TEST(social_avatar_rejects_bad_type_and_size) {
    Env e;
    SbResult res;
    e.social.uploadAvatar("GIF89a......", [&](const SbResult& r, const SocialProfile&) { res = r; });
    CHECK_EQ(res.error, std::string("bad_image_type"));
    CHECK(e.http.pending.empty());
    std::string big = std::string("\x89PNG\r\n\x1a\n", 8) + std::string(SocialService::kMaxAvatarBytes, 'x');
    e.social.uploadAvatar(big, [&](const SbResult& r, const SocialProfile&) { res = r; });
    CHECK_EQ(res.error, std::string("image_too_large"));
    CHECK_EQ(SocialService::sniffImageType(std::string("RIFF\x10\x00\x00\x00WEBPVP8 ", 16)), std::string("image/webp"));
}

TEST(social_dm_open_messages_seen) {
    Env e;
    std::string chat;
    UserCard peer;
    e.social.openDm("b", [&](const SbResult& r, const std::string& c, const UserCard& p, const std::string&) { chat = c; peer = p; });
    e.http.respond(200, R"({"chat_id":"c1","peer":{"id":"b","username":"bob","display_name":"Bob"},"peer_last_read":null})");
    CHECK_EQ(chat, std::string("c1"));
    CHECK_EQ(peer.name(), std::string("Bob"));
    std::vector<DmMessage> msgs;
    e.social.dmMessages("c1", [&](const SbResult&, const std::vector<DmMessage>& v, const std::string&) { msgs = v; });
    e.http.respond(200, R"({"messages":[
      {"id":"1","author":"me","text":"a","created_at":"2026-11-18T10:00:00+00:00"},
      {"id":"2","author":"b","text":"b","created_at":"2026-11-18T10:01:00+00:00"},
      {"id":"3","author":"me","text":"c","created_at":"2026-11-18T10:02:00.500+00:00"}],
      "peer_last_read":"2026-11-18T10:01:30+00:00"})");
    CHECK_EQ(msgs.size(), (size_t)3);
    CHECK(msgs[0].state == DmMessage::State::Seen);
    CHECK(msgs[2].state == DmMessage::State::Sent);
    SocialService::applySeen(msgs, "me", "2026-11-18T10:05:00+00:00");
    CHECK(msgs[2].state == DmMessage::State::Seen);
}

TEST(social_time_utc_parse_and_local_format) {
    CHECK_EQ(SocialService::parseIsoUtc("1970-01-01T00:00:00Z"), (int64_t)0 + 0);
    CHECK_EQ(SocialService::parseIsoUtc("2026-11-18T14:03:05.123456+00:00"), (int64_t)1795010585);
    CHECK_EQ(SocialService::parseIsoUtc("2026-11-18T17:33:05+03:30"), (int64_t)1795010585);
    CHECK_EQ(SocialService::parseIsoUtc("2026-11-18T09:03:05-05:00"), (int64_t)1795010585);
    CHECK_EQ(SocialService::formatLocal(1795010585, "relative", false, 1795010585 + 30), std::string("now"));
    CHECK_EQ(SocialService::formatLocal(1795010585, "relative", false, 1795010585 + 7200), std::string("2h"));
    CHECK_EQ(SocialService::formatLocal(1795010585, "relative", true, 1795010585 + 300), std::string("۵ دقیقه پیش"));
    setenv("TZ", "Asia/Tehran", 1);
    tzset();
    CHECK_EQ(SocialService::formatLocal(1795010585, "datetime", false, 0), std::string("2026-11-18 17:33"));
    CHECK_EQ(SocialService::formatLocal(1795010585, "date", true, 0), std::string("۱۴۰۵/۰۸/۲۷"));
    setenv("TZ", "America/New_York", 1);  // DST ends Nov 1 2026 -> EST (UTC-5)
    tzset();
    CHECK_EQ(SocialService::formatLocal(1795010585, "time", false, 0), std::string("09:03"));
    CHECK_EQ(SocialService::formatLocal(SocialService::parseIsoUtc("2026-07-01T12:00:00Z"), "time", false, 0), std::string("08:00"));
    unsetenv("TZ");
    tzset();
}

TEST(social_deep_link_params) {
    auto p = OnlineService::parseLinkParams("nexora://login-callback#access_token=a.b.c&expires_in=3600&refresh_token=rt%2B1&type=recovery");
    CHECK_EQ(p["refresh_token"], std::string("rt+1"));
    CHECK_EQ(p["type"], std::string("recovery"));
    auto q = OnlineService::parseLinkParams("nexora://login-callback?error=access_denied&error_description=Email+link+is+invalid");
    CHECK_EQ(q["error_description"], std::string("Email link is invalid"));
}

TEST(social_username_login_uses_edge_function) {
    SHttp http;
    MemoryStore st;
    OnlineConfig cfg{"https://main.supabase.co", "anon", "", "", ""};
    OnlineService os(cfg, &http, &st, [] { return std::unique_ptr<IWebSocket>(new NoWs()); }, [] { return sNow; });
    bool ok = false;
    os.signInEmail(" Bob ", "password1", [&](bool b) { ok = b; });
    HttpRequest q = http.respond(200, R"({"access_token":"T2","refresh_token":"R2","expires_in":3600,"user":{"id":"b","email":"bob@x.io"}})");
    CHECK_EQ(q.url, std::string("https://main.supabase.co/functions/v1/nx-login"));
    CHECK_EQ(Json::parse(q.body)["username"].asString(), std::string("bob"));
    CHECK(ok);
    CHECK_EQ(os.uid(), std::string("b"));
}
