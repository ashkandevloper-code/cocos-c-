// Social network client: profiles, avatars, follow / follow requests, private accounts, search, user posts,
// direct messages and notifications. Talks only to the nx_social_* RPCs (supabase/migrations/
// 20261118300000_nx_social_v2.sql); the database is the source of truth, nothing is kept only on the device.
// Engine-independent (unit-tested on the host with a fake HTTP transport).
#pragma once
#include "backend/OnlineService.h"
#include "core/Json.h"

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace nx {

struct UserCard {
    std::string id, username, displayName, avatarUrl;
    bool isPrivate = false, isFollowing = false, isPending = false, isMe = false;
    std::string since;  // follow date in lists, request date in requests (ISO UTC)
    static UserCard fromJson(const Json& j);
    std::string name() const { return displayName.empty() ? username : displayName; }
};

enum class FollowState { None, Following, Requested, Self };

struct SocialProfile {
    UserCard user;
    std::string bio, website, location, createdAt, lastActive;
    int followers = 0, following = 0, posts = 0, pendingRequests = 0;
    bool followsMe = false, isBlocked = false, canView = true;
    bool loaded = false;
    static SocialProfile fromJson(const Json& j);
    FollowState followState() const;
};

struct SocialPost {
    std::string id, text, imageUrl, createdAt;
    int likes = 0, comments = 0;
    static SocialPost fromJson(const Json& j);
};

struct DmMessage {
    enum class State { Sending, Sent, Seen, Failed };
    std::string id, localId, author, text, createdAt;
    bool deleted = false;
    State state = State::Sent;
    static DmMessage fromJson(const Json& j);
};

struct SocialNotification {
    std::string id, kind, entityId, createdAt;
    bool read = false;
    UserCard actor;
    static SocialNotification fromJson(const Json& j);
};

struct ChatSummary {
    std::string id, kind, name, peerId, peerName, peerAvatar, lastText, lastAt;
    int unread = 0;
    static ChatSummary fromJson(const Json& j);
};

class SocialService {
public:
    explicit SocialService(OnlineService* online) : on_(online) {}

    bool signedIn() const { return on_ && on_->isOnline() && !on_->uid().empty(); }
    std::string myId() const { return on_ ? on_->uid() : ""; }

    // ---- profile ----
    void profile(const std::string& userId, std::function<void(const SbResult&, const SocialProfile&)> cb);
    void saveProfile(const std::string& displayName, const std::string& username, const std::string& bio,
                     const std::string& website, const std::string& location,
                     std::function<void(const SbResult&, const SocialProfile&)> cb);
    void setPrivacy(bool isPrivate, std::function<void(const SbResult&, const SocialProfile&)> cb);

    /// Uploads already-compressed image bytes (JPEG/PNG/WEBP, <= 2 MB) to bucket "avatars" under <uid>/,
    /// points the profile at it and deletes the previous file.
    void uploadAvatar(const std::string& bytes, std::function<void(const SbResult&, const SocialProfile&)> cb);
    void removeAvatar(std::function<void(const SbResult&, const SocialProfile&)> cb);
    /// "image/jpeg" | "image/png" | "image/webp" by magic bytes, "" when the data is not one of those.
    static std::string sniffImageType(const std::string& bytes);
    static constexpr size_t kMaxAvatarBytes = 2 * 1024 * 1024;

    // ---- follow ----
    /// Optimistic local state change: returns the profile as it looks right after tapping Follow / Unfollow.
    static SocialProfile optimisticFollow(const SocialProfile& p, bool follow);
    /// Follow / unfollow / request / cancel request. cb gets the server's profile, or the error.
    void setFollow(const std::string& userId, bool follow, std::function<void(const SbResult&, const SocialProfile&)> cb);
    void removeFollower(const std::string& userId, std::function<void(const SbResult&)> cb);
    /// kind: "followers" | "following"
    void followList(const std::string& userId, const std::string& kind, int offset,
                    std::function<void(const SbResult&, const std::vector<UserCard>&)> cb);
    void followRequests(std::function<void(const SbResult&, const std::vector<UserCard>&)> cb);
    void respondRequest(const std::string& requester, bool accept, std::function<void(const SbResult&)> cb);
    void search(const std::string& q, std::function<void(const SbResult&, const std::vector<UserCard>&)> cb);
    void userPosts(const std::string& userId, std::function<void(const SbResult&, const std::vector<SocialPost>&)> cb);

    // ---- messages ----
    void openDm(const std::string& userId,
                std::function<void(const SbResult&, const std::string& chatId, const UserCard& peer, const std::string& peerLastRead)> cb);
    void dmMessages(const std::string& chatId,
                    std::function<void(const SbResult&, const std::vector<DmMessage>&, const std::string& peerLastRead)> cb);
    void sendDm(const std::string& chatId, const std::string& text, std::function<void(const SbResult&, const DmMessage&)> cb);
    void markChatRead(const std::string& chatId);
    void myChats(std::function<void(const SbResult&, const std::vector<ChatSummary>&)> cb);
    /// Marks Sent messages of mine as Seen when the peer's read position reached them.
    static void applySeen(std::vector<DmMessage>& msgs, const std::string& myId, const std::string& peerLastRead);

    // ---- notifications ----
    void notifications(std::function<void(const SbResult&, const std::vector<SocialNotification>&, int unread)> cb);
    void markAllNotificationsRead();

    // ---- helpers ----
    /// Maps server / transport errors to a user-facing message (Persian or English).
    static std::string friendlyError(const SbResult& r, bool fa);
    /// Parses "2026-11-18T14:03:05.123+00:00" / "...Z" to unix seconds (UTC). Returns 0 on failure.
    static int64_t parseIsoUtc(const std::string& iso);
    /// Formats a UTC instant in the device time zone (DST-aware via the C library's local time rules).
    /// style: "time" (14:03), "date" (2026/11/18), "datetime", "relative" (now, 5m, 3h, 2d, else date).
    static std::string formatLocal(int64_t utcSec, const std::string& style, bool fa, int64_t nowSec);
    static std::string persianDigits(const std::string& s);

private:
    OnlineService* on_;
};

}  // namespace nx
