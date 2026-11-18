// Social network screens (cocos2d-x): profile (own / others), edit profile + avatar, followers / following,
// follow requests, search, inbox, direct chat, notifications, change password. All data comes from
// SocialService (Supabase); scenes only render it and never invent values.
#pragma once
#include "backend/SocialService.h"
#include "cocos2d.h"
#include "ui/CocosGUI.h"

#include <memory>
#include <string>
#include <vector>

namespace nx {

namespace ui { class VList; class NxButton; class Avatar; }

/// Base for screens with async work: [alive()] turns false when the scene is destroyed, so late network
/// callbacks never touch freed nodes.
class SocialSceneBase : public cocos2d::Scene {
public:
    ~SocialSceneBase() override { *alive_ = false; }

protected:
    std::shared_ptr<bool> alive() const { return alive_; }
    void buildBackground();
    /// Content area below the top bar.
    cocos2d::Size bodySize(float bottomReserve = 0) const;
    void showLoading(ui::VList* list, int rows = 4);
    void showError(ui::VList* list, const std::string& msg, std::function<void()> retry);
    void showEmpty(ui::VList* list, const std::string& title, const std::string& sub);
    std::shared_ptr<bool> alive_ = std::make_shared<bool>(true);
};

class ProfileScene : public SocialSceneBase {
public:
    static ProfileScene* create(const std::string& userId);
    void onEnter() override;

private:
    bool initWith(const std::string& userId);
    void load();
    void render();
    void renderPosts();
    void tapFollow();
    void applyFollow(bool follow);
    std::string userId_;
    SocialProfile p_;
    std::vector<SocialPost> posts_;
    bool postsLoaded_ = false, postsError_ = false, busy_ = false, firstEnter_ = true;
    ui::VList* list_ = nullptr;
    cocos2d::Node* bar_ = nullptr;
};

class EditProfileScene : public SocialSceneBase {
public:
    static EditProfileScene* create(const SocialProfile& p);

private:
    bool initWith(const SocialProfile& p);
    void build();
    void pickAvatar();
    void removeAvatar();
    void save();
    void togglePrivate();
    SocialProfile p_;
    ui::Avatar* avatar_ = nullptr;
    cocos2d::ui::EditBox *name_ = nullptr, *username_ = nullptr, *bio_ = nullptr, *website_ = nullptr, *location_ = nullptr;
    cocos2d::Label* bioCount_ = nullptr;
    ui::NxButton *saveBtn_ = nullptr, *privBtn_ = nullptr, *photoBtn_ = nullptr;
    bool busy_ = false;
};

class FollowListScene : public SocialSceneBase {
public:
    /// kind: "followers" | "following" | "requests"
    static FollowListScene* create(const std::string& userId, const std::string& kind, const std::string& title);
    void onEnter() override;

private:
    bool initWith(const std::string& userId, const std::string& kind, const std::string& title);
    void load(bool more);
    void render();
    std::string userId_, kind_;
    std::vector<UserCard> rows_;
    bool hasMore_ = false, loading_ = false, firstEnter_ = true;
    ui::VList* list_ = nullptr;
};

class SocialHomeScene : public SocialSceneBase {
public:
    /// tab: "profile" (returns the own ProfileScene) | "search" | "inbox" | "alerts"
    static cocos2d::Scene* createAt(const std::string& tab);
    void onEnter() override;
    void onExit() override;

private:
    bool initWith(const std::string& tab);
    void selectTab(const std::string& tab);
    void buildTabBar();
    void renderSearch();
    void runSearch(const std::string& q);
    void renderInbox();
    void renderAlerts();
    void refreshBadges();
    std::string tab_;
    cocos2d::Node* content_ = nullptr;
    cocos2d::Node* tabBar_ = nullptr;
    ui::VList* list_ = nullptr;
    cocos2d::ui::EditBox* searchBox_ = nullptr;
    std::string lastQuery_;
    int searchSeq_ = 0;
    int unreadAlerts_ = 0, unreadChats_ = 0;
};

class ChatScene : public SocialSceneBase {
public:
    static ChatScene* create(const std::string& peerId);
    void onEnter() override;
    void onExit() override;

private:
    bool initWith(const std::string& peerId);
    void open();
    void reload();
    void render(bool keepBottom = true);
    void send();
    void subscribe();
    void poll(float);
    std::string peerId_, chatId_, peerLastRead_;
    UserCard peer_;
    std::vector<DmMessage> msgs_;
    ui::VList* list_ = nullptr;
    cocos2d::Node* header_ = nullptr;
    cocos2d::ui::EditBox* input_ = nullptr;
    std::shared_ptr<RealtimeChannel> channel_;
    bool loaded_ = false;
    int localSeq_ = 0;
};

class PostScene : public SocialSceneBase {
public:
    static PostScene* create(const SocialPost& post, const UserCard& author);

private:
    bool initWith(const SocialPost& post, const UserCard& author);
};

class ChangePasswordScene : public SocialSceneBase {
public:
    /// recovery = opened from a password-reset e-mail link (no current password needed).
    static ChangePasswordScene* create(bool recovery);

private:
    bool initWith(bool recovery);
    void submit();
    bool recovery_ = false, busy_ = false;
    cocos2d::ui::EditBox *current_ = nullptr, *pw_ = nullptr, *pw2_ = nullptr;
    cocos2d::Label* error_ = nullptr;
};

class SignInRequiredScene : public SocialSceneBase {
public:
    CREATE_FUNC(SignInRequiredScene);
    bool init() override;
};

/// Modal confirm sheet on the running scene.
void showConfirm(const std::string& title, const std::string& confirmLabel, std::function<void()> onConfirm, bool danger = true);

}  // namespace nx
