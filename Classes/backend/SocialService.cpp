#include "backend/SocialService.h"

#include <cstdio>
#include <cstdlib>
#include <ctime>

namespace nx {

namespace {

std::string s(const Json& j, const char* k) { return j[k].isString() ? j[k].asString() : (j[k].isNull() ? "" : j[k].str()); }
int n(const Json& j, const char* k) { return static_cast<int>(j[k].asInt(0)); }
bool b(const Json& j, const char* k) { return j[k].asBool(false); }

bool contains(const std::string& h, const char* needle) { return h.find(needle) != std::string::npos; }

void toCards(const Json& arr, std::vector<UserCard>& out) {
    if (!arr.isArray()) return;
    for (auto& x : arr.items()) out.push_back(UserCard::fromJson(x));
}

int64_t daysFromCivil(int64_t y, unsigned m, unsigned d) {  // Howard Hinnant's algorithm (UTC, no time zone)
    y -= m <= 2;
    const int64_t era = (y >= 0 ? y : y - 399) / 400;
    const unsigned yoe = static_cast<unsigned>(y - era * 400);
    const unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + static_cast<int64_t>(doe) - 719468;
}

// Gregorian -> Jalali (Solar Hijri), the calendar Persian users expect.
void toJalali(int gy, int gm, int gd, int& jy, int& jm, int& jd) {
    static const int g_d_m[12] = {0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334};
    int gy2 = gm > 2 ? gy + 1 : gy;
    long days = 355666 + (365L * gy) + ((gy2 + 3) / 4) - ((gy2 + 99) / 100) + ((gy2 + 399) / 400) + gd + g_d_m[gm - 1];
    jy = -1595 + (33 * (days / 12053));
    days %= 12053;
    jy += 4 * (days / 1461);
    days %= 1461;
    if (days > 365) {
        jy += (days - 1) / 365;
        days = (days - 1) % 365;
    }
    if (days < 186) {
        jm = 1 + days / 31;
        jd = 1 + days % 31;
    } else {
        jm = 7 + (days - 186) / 30;
        jd = 1 + (days - 186) % 30;
    }
}

}  // namespace

// ------------------------------------------------------------------ models

UserCard UserCard::fromJson(const Json& j) {
    UserCard c;
    c.id = s(j, "id");
    c.username = s(j, "username");
    c.displayName = s(j, "display_name");
    c.avatarUrl = s(j, "avatar_url");
    c.isPrivate = b(j, "is_private");
    c.isFollowing = b(j, "is_following");
    c.isPending = b(j, "is_pending");
    c.isMe = b(j, "is_me");
    c.since = j.has("since") ? s(j, "since") : s(j, "requested_at");
    return c;
}

SocialProfile SocialProfile::fromJson(const Json& j) {
    SocialProfile p;
    p.user = UserCard::fromJson(j);
    p.bio = s(j, "bio");
    p.website = s(j, "website");
    p.location = s(j, "location");
    p.createdAt = s(j, "created_at");
    p.lastActive = s(j, "last_active");
    p.followers = n(j, "followers");
    p.following = n(j, "following");
    p.posts = n(j, "posts");
    p.pendingRequests = n(j, "pending_requests");
    p.followsMe = b(j, "follows_me");
    p.isBlocked = b(j, "is_blocked");
    p.canView = j.has("can_view") ? b(j, "can_view") : true;
    p.loaded = !p.user.id.empty();
    return p;
}

FollowState SocialProfile::followState() const {
    if (user.isMe) return FollowState::Self;
    if (user.isFollowing) return FollowState::Following;
    if (user.isPending) return FollowState::Requested;
    return FollowState::None;
}

SocialPost SocialPost::fromJson(const Json& j) {
    SocialPost p;
    p.id = s(j, "id");
    p.text = s(j, "text");
    p.createdAt = s(j, "created_at");
    p.likes = n(j, "like_count");
    p.comments = n(j, "comment_count");
    const Json& v = j["visual"];
    if (v.isObject()) {
        for (const char* k : {"image_url", "url", "image", "thumb"}) {
            if (v[k].isString() && v[k].asString().rfind("http", 0) == 0) { p.imageUrl = v[k].asString(); break; }
        }
    }
    return p;
}

DmMessage DmMessage::fromJson(const Json& j) {
    DmMessage m;
    m.id = s(j, "id");
    m.author = s(j, "author");
    m.text = s(j, "text");
    m.createdAt = s(j, "created_at");
    m.deleted = b(j, "deleted");
    m.state = DmMessage::State::Sent;
    return m;
}

SocialNotification SocialNotification::fromJson(const Json& j) {
    SocialNotification x;
    x.id = s(j, "id");
    x.kind = s(j, "kind");
    x.entityId = s(j, "entity_id");
    x.createdAt = s(j, "created_at");
    x.read = b(j, "read");
    if (j["actor"].isObject()) x.actor = UserCard::fromJson(j["actor"]);
    return x;
}

ChatSummary ChatSummary::fromJson(const Json& j) {
    ChatSummary c;
    c.id = s(j, "id");
    c.kind = s(j, "kind");
    c.name = s(j, "name");
    c.peerId = s(j, "peer");
    c.peerName = s(j, "peer_name");
    c.peerAvatar = s(j, "peer_avatar");
    c.unread = n(j, "unread");
    c.lastAt = s(j, "last_msg_at");
    const Json& last = j["last"];
    if (last.isObject()) {
        c.lastText = s(last, "text");
        c.lastAt = s(last, "created_at");
    }
    return c;
}

// ------------------------------------------------------------------ profile

void SocialService::profile(const std::string& userId, std::function<void(const SbResult&, const SocialProfile&)> cb) {
    on_->rpc("nx_social_profile", Json::obj({{"p_user", userId}}),
             [cb](const SbResult& r) { cb(r, r.ok ? SocialProfile::fromJson(r.data) : SocialProfile()); });
}

void SocialService::saveProfile(const std::string& displayName, const std::string& username, const std::string& bio,
                                const std::string& website, const std::string& location,
                                std::function<void(const SbResult&, const SocialProfile&)> cb) {
    on_->rpc("nx_social_save_profile",
             Json::obj({{"p_display_name", displayName}, {"p_username", username}, {"p_bio", bio},
                        {"p_website", website}, {"p_location", location}}),
             [cb](const SbResult& r) { cb(r, r.ok ? SocialProfile::fromJson(r.data) : SocialProfile()); });
}

void SocialService::setPrivacy(bool isPrivate, std::function<void(const SbResult&, const SocialProfile&)> cb) {
    on_->rpc("nx_social_set_privacy", Json::obj({{"p_private", isPrivate}}),
             [cb](const SbResult& r) { cb(r, r.ok ? SocialProfile::fromJson(r.data) : SocialProfile()); });
}

std::string SocialService::sniffImageType(const std::string& d) {
    if (d.size() >= 3 && static_cast<unsigned char>(d[0]) == 0xFF && static_cast<unsigned char>(d[1]) == 0xD8 &&
        static_cast<unsigned char>(d[2]) == 0xFF)
        return "image/jpeg";
    if (d.size() >= 8 && d.compare(0, 8, "\x89PNG\r\n\x1a\n") == 0) return "image/png";
    if (d.size() >= 12 && d.compare(0, 4, "RIFF") == 0 && d.compare(8, 4, "WEBP") == 0) return "image/webp";
    return "";
}

void SocialService::uploadAvatar(const std::string& bytes, std::function<void(const SbResult&, const SocialProfile&)> cb) {
    SbResult bad;
    std::string type = sniffImageType(bytes);
    if (type.empty()) { bad.error = "bad_image_type"; cb(bad, SocialProfile()); return; }
    if (bytes.size() > kMaxAvatarBytes) { bad.error = "image_too_large"; cb(bad, SocialProfile()); return; }
    std::string me = myId();
    if (me.empty()) { bad.error = "offline"; cb(bad, SocialProfile()); return; }
    std::string ext = type == "image/jpeg" ? "jpg" : type == "image/png" ? "png" : "webp";
    std::string path = me + "/av_" + std::to_string(systemNowMs()) + "." + ext;
    SupabaseClient* c = on_->client();
    c->ensureFreshToken([this, c, bytes, type, path, cb](bool ok) {
        if (!ok) { SbResult e; e.error = "offline"; cb(e, SocialProfile()); return; }
        c->storageUpload("avatars", path, bytes, type, false, [this, c, path, cb](const SbResult& up) {
            if (!up.ok) { cb(up, SocialProfile()); return; }
            on_->rpc("nx_social_set_avatar", Json::obj({{"p_path", path}}), [c, path, cb](const SbResult& r) {
                if (!r.ok) {
                    c->storageRemove("avatars", {path}, [](const SbResult&) {});
                    cb(r, SocialProfile());
                    return;
                }
                std::string old = r.data["old_path"].isString() ? r.data["old_path"].asString() : "";
                if (!old.empty() && old != path) c->storageRemove("avatars", {old}, [](const SbResult&) {});
                cb(r, SocialProfile::fromJson(r.data["profile"]));
            });
        });
    });
}

void SocialService::removeAvatar(std::function<void(const SbResult&, const SocialProfile&)> cb) {
    SupabaseClient* c = on_->client();
    on_->rpc("nx_social_set_avatar", Json::obj({{"p_path", nullptr}}), [c, cb](const SbResult& r) {
        if (r.ok && r.data["old_path"].isString() && !r.data["old_path"].asString().empty())
            c->storageRemove("avatars", {r.data["old_path"].asString()}, [](const SbResult&) {});
        cb(r, r.ok ? SocialProfile::fromJson(r.data["profile"]) : SocialProfile());
    });
}

// ------------------------------------------------------------------ follow

SocialProfile SocialService::optimisticFollow(const SocialProfile& p, bool follow) {
    SocialProfile o = p;
    if (o.user.isMe) return o;
    if (follow) {
        if (o.user.isFollowing || o.user.isPending) return o;
        if (o.user.isPrivate) {
            o.user.isPending = true;
        } else {
            o.user.isFollowing = true;
            o.followers += 1;
            o.canView = true;
        }
    } else {
        if (o.user.isFollowing) {
            o.user.isFollowing = false;
            o.followers = o.followers > 0 ? o.followers - 1 : 0;
            if (o.user.isPrivate) o.canView = false;
        }
        o.user.isPending = false;
    }
    return o;
}

void SocialService::setFollow(const std::string& userId, bool follow,
                              std::function<void(const SbResult&, const SocialProfile&)> cb) {
    on_->rpc("nx_social_toggle_follow", Json::obj({{"p_target", userId}, {"p_follow", follow}}),
             [cb](const SbResult& r) { cb(r, r.ok ? SocialProfile::fromJson(r.data) : SocialProfile()); });
}

void SocialService::removeFollower(const std::string& userId, std::function<void(const SbResult&)> cb) {
    on_->rpc("nx_social_remove_follower", Json::obj({{"p_user", userId}}), cb);
}

void SocialService::followList(const std::string& userId, const std::string& kind, int offset,
                               std::function<void(const SbResult&, const std::vector<UserCard>&)> cb) {
    on_->rpc("nx_social_follow_list",
             Json::obj({{"p_user", userId}, {"p_kind", kind}, {"p_limit", 50}, {"p_offset", offset}}),
             [cb](const SbResult& r) {
                 std::vector<UserCard> v;
                 if (r.ok) toCards(r.data, v);
                 cb(r, v);
             });
}

void SocialService::followRequests(std::function<void(const SbResult&, const std::vector<UserCard>&)> cb) {
    on_->rpc("nx_social_follow_requests", Json::obj({{"p_limit", 100}, {"p_offset", 0}}), [cb](const SbResult& r) {
        std::vector<UserCard> v;
        if (r.ok) toCards(r.data, v);
        cb(r, v);
    });
}

void SocialService::respondRequest(const std::string& requester, bool accept, std::function<void(const SbResult&)> cb) {
    on_->rpc("nx_social_respond_follow_request", Json::obj({{"p_requester", requester}, {"p_accept", accept}}), cb);
}

void SocialService::search(const std::string& q, std::function<void(const SbResult&, const std::vector<UserCard>&)> cb) {
    on_->rpc("nx_social_search", Json::obj({{"p_q", q}, {"p_limit", 30}}), [cb](const SbResult& r) {
        std::vector<UserCard> v;
        if (r.ok) toCards(r.data, v);
        cb(r, v);
    });
}

void SocialService::userPosts(const std::string& userId, std::function<void(const SbResult&, const std::vector<SocialPost>&)> cb) {
    on_->rpc("nx_social_user_posts", Json::obj({{"p_user", userId}, {"p_limit", 30}}), [cb](const SbResult& r) {
        std::vector<SocialPost> v;
        if (r.ok && r.data.isArray())
            for (auto& x : r.data.items()) v.push_back(SocialPost::fromJson(x));
        cb(r, v);
    });
}

// ------------------------------------------------------------------ messages

void SocialService::openDm(const std::string& userId,
                           std::function<void(const SbResult&, const std::string&, const UserCard&, const std::string&)> cb) {
    on_->rpc("nx_social_open_dm", Json::obj({{"p_user", userId}}), [cb](const SbResult& r) {
        if (!r.ok) { cb(r, "", UserCard(), ""); return; }
        cb(r, s(r.data, "chat_id"), UserCard::fromJson(r.data["peer"]), s(r.data, "peer_last_read"));
    });
}

void SocialService::dmMessages(const std::string& chatId,
                               std::function<void(const SbResult&, const std::vector<DmMessage>&, const std::string&)> cb) {
    on_->rpc("nx_social_dm_messages", Json::obj({{"p_chat", chatId}, {"p_limit", 100}}), [this, cb](const SbResult& r) {
        std::vector<DmMessage> v;
        std::string seen;
        if (r.ok) {
            for (auto& x : r.data["messages"].items()) v.push_back(DmMessage::fromJson(x));
            seen = s(r.data, "peer_last_read");
            applySeen(v, myId(), seen);
        }
        cb(r, v, seen);
    });
}

void SocialService::sendDm(const std::string& chatId, const std::string& text, std::function<void(const SbResult&, const DmMessage&)> cb) {
    on_->rpc("nx_social_send_dm", Json::obj({{"p_chat", chatId}, {"p_text", text}}),
             [cb](const SbResult& r) { cb(r, r.ok ? DmMessage::fromJson(r.data) : DmMessage()); });
}

void SocialService::markChatRead(const std::string& chatId) {
    on_->rpc("nx_social_mark_chat_read", Json::obj({{"p_chat", chatId}}), [](const SbResult&) {});
}

void SocialService::myChats(std::function<void(const SbResult&, const std::vector<ChatSummary>&)> cb) {
    on_->rpc("nx_my_chats", Json::object(), [cb](const SbResult& r) {
        std::vector<ChatSummary> v;
        if (r.ok && r.data.isArray())
            for (auto& x : r.data.items()) v.push_back(ChatSummary::fromJson(x));
        cb(r, v);
    });
}

void SocialService::applySeen(std::vector<DmMessage>& msgs, const std::string& myId, const std::string& peerLastRead) {
    int64_t seen = parseIsoUtc(peerLastRead);
    for (auto& m : msgs) {
        if (m.author != myId || m.state == DmMessage::State::Sending || m.state == DmMessage::State::Failed) continue;
        int64_t at = parseIsoUtc(m.createdAt);
        m.state = (seen > 0 && at > 0 && at <= seen) ? DmMessage::State::Seen : DmMessage::State::Sent;
    }
}

// ------------------------------------------------------------------ notifications

void SocialService::notifications(std::function<void(const SbResult&, const std::vector<SocialNotification>&, int)> cb) {
    on_->rpc("nx_social_notifications_list", Json::obj({{"p_limit", 60}}), [cb](const SbResult& r) {
        std::vector<SocialNotification> v;
        int unread = 0;
        if (r.ok) {
            for (auto& x : r.data["items"].items()) v.push_back(SocialNotification::fromJson(x));
            unread = n(r.data, "unread");
        }
        cb(r, v, unread);
    });
}

void SocialService::markAllNotificationsRead() {
    on_->rpc("nx_social_mark_all_notifications", Json::object(), [](const SbResult&) {});
}

// ------------------------------------------------------------------ helpers

std::string SocialService::friendlyError(const SbResult& r, bool fa) {
    const std::string& e = r.error;
    auto t = [fa](const char* f, const char* en) { return std::string(fa ? f : en); };
    if (e == "network" || r.status == 0) return t("اتصال اینترنت را بررسی کن.", "Check your internet connection.");
    if (e == "offline" || contains(e, "not_signed_in") || contains(e, "JWT"))
        return t("برای این کار باید وارد حساب شوی.", "Sign in to do this.");
    if (contains(e, "rate_limited")) return t("خیلی سریع! کمی صبر کن و دوباره امتحان کن.", "Slow down a little and try again.");
    if (contains(e, "username_taken")) return t("این نام کاربری قبلاً گرفته شده.", "That username is taken.");
    if (contains(e, "bad_username"))
        return t("نام کاربری ۳ تا ۲۰ حرف انگلیسی کوچک، عدد، _ یا . باشد.", "Username: 3–20 lowercase letters, digits, _ or .");
    if (contains(e, "bio_too_long")) return t("بیو حداکثر ۱۶۰ حرف است.", "Bio can be at most 160 characters.");
    if (contains(e, "bad_website")) return t("آدرس وب‌سایت معتبر نیست.", "That website address is not valid.");
    if (contains(e, "private_account")) return t("این حساب خصوصی است.", "This account is private.");
    if (contains(e, "blocked")) return t("امکان این کار با این کاربر وجود ندارد.", "You can't do that with this user.");
    if (contains(e, "profile_not_found")) return t("این کاربر پیدا نشد.", "User not found.");
    if (contains(e, "bad_target")) return t("نمی‌توانی خودت را دنبال کنی.", "You can't follow yourself.");
    if (contains(e, "request_not_found")) return t("این درخواست دیگر وجود ندارد.", "That request no longer exists.");
    if (e == "bad_image_type") return t("فقط عکس JPG، PNG یا WEBP.", "Only JPG, PNG or WEBP images.");
    if (e == "image_too_large" || contains(e, "exceeded the maximum") || r.status == 413)
        return t("حجم عکس زیاد است (حداکثر ۲ مگابایت).", "The image is too large (max 2 MB).");
    if (contains(e, "mime type") || contains(e, "invalid_mime")) return t("فرمت عکس پشتیبانی نمی‌شود.", "Unsupported image format.");
    if (contains(e, "not_member")) return t("به این گفتگو دسترسی نداری.", "You are not in this conversation.");
    if (contains(e, "empty")) return t("پیام خالی است.", "The message is empty.");
    if (e == "invalid_login") return t("نام کاربری/ایمیل یا رمز نادرست است.", "Wrong username/e-mail or password.");
    if (e == "weak_password") return t("رمز باید حداقل ۸ کاراکتر باشد.", "Password must be at least 8 characters.");
    if (e == "email_taken") return t("این ایمیل قبلاً ثبت شده؛ وارد شو.", "That e-mail is already registered; sign in.");
    return t("خطایی رخ داد. دوباره امتحان کن.", "Something went wrong. Try again.");
}

int64_t SocialService::parseIsoUtc(const std::string& iso) {
    int Y, M, D, h = 0, m = 0;
    double sec = 0;
    if (iso.size() < 10 || std::sscanf(iso.c_str(), "%4d-%2d-%2d", &Y, &M, &D) != 3) return 0;
    if (iso.size() >= 19) std::sscanf(iso.c_str() + 11, "%2d:%2d:%lf", &h, &m, &sec);
    int64_t t = daysFromCivil(Y, static_cast<unsigned>(M), static_cast<unsigned>(D)) * 86400 + h * 3600 + m * 60 +
                static_cast<int64_t>(sec);
    // Offset suffix: Z | +HH:MM | -HH:MM (after the seconds/fraction).
    size_t p = iso.find_first_of("Z+-", 19);
    if (p != std::string::npos && iso[p] != 'Z' && p + 3 <= iso.size()) {
        int oh = std::atoi(iso.substr(p + 1, 2).c_str());
        int om = iso.size() >= p + 6 ? std::atoi(iso.substr(p + 4, 2).c_str()) : 0;
        int off = oh * 3600 + om * 60;
        t += iso[p] == '+' ? -off : off;
    }
    return t;
}

std::string SocialService::persianDigits(const std::string& in) {
    static const char* d[] = {"۰", "۱", "۲", "۳", "۴", "۵", "۶", "۷", "۸", "۹"};
    std::string o;
    for (char c : in) {
        if (c >= '0' && c <= '9') o += d[c - '0'];
        else o += c;
    }
    return o;
}

std::string SocialService::formatLocal(int64_t utcSec, const std::string& style, bool fa, int64_t nowSec) {
    if (utcSec <= 0) return "";
    std::time_t tt = static_cast<std::time_t>(utcSec);
    std::tm lt{};
#if defined(_WIN32)
    localtime_s(&lt, &tt);
#else
    localtime_r(&tt, &lt);  // device zone incl. daylight-saving rules
#endif
    char buf[64];
    auto date = [&]() {
        if (fa) {
            int jy, jm, jd;
            toJalali(lt.tm_year + 1900, lt.tm_mon + 1, lt.tm_mday, jy, jm, jd);
            std::snprintf(buf, sizeof buf, "%04d/%02d/%02d", jy, jm, jd);
        } else {
            std::snprintf(buf, sizeof buf, "%04d-%02d-%02d", lt.tm_year + 1900, lt.tm_mon + 1, lt.tm_mday);
        }
        return std::string(buf);
    };
    auto time = [&]() {
        std::snprintf(buf, sizeof buf, "%02d:%02d", lt.tm_hour, lt.tm_min);
        return std::string(buf);
    };
    std::string out;
    if (style == "time") out = time();
    else if (style == "date") out = date();
    else if (style == "datetime") out = date() + " " + time();
    else {
        int64_t d = nowSec - utcSec;
        if (d < 60) return fa ? "همین حالا" : "now";
        if (d < 3600) out = std::to_string(d / 60) + (fa ? " دقیقه پیش" : "m");
        else if (d < 86400) out = std::to_string(d / 3600) + (fa ? " ساعت پیش" : "h");
        else if (d < 7 * 86400) out = std::to_string(d / 86400) + (fa ? " روز پیش" : "d");
        else out = date();
    }
    return fa ? persianDigits(out) : out;
}

}  // namespace nx
