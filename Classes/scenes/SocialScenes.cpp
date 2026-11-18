#include "scenes/SocialScenes.h"

#include "app/NxApp.h"
#include "core/AppSettings.h"
#include "platform/NativeBridge.h"
#include "scenes/ShellScenes.h"
#include "ui/Media.h"
#include "ui/Widgets.h"

#include <algorithm>
#include <regex>

USING_NS_CC;
using namespace nx::ui;

namespace nx {

namespace {

SocialService& svc() { return NxApp::get().social(); }
bool fa() { return uiIsFa(); }
int64_t nowSec() { return systemNowMs() / 1000; }

/// Mirrors an x coordinate for RTL layouts.
float mx(float x, float w) { return fa() ? w - x : x; }

std::string ellipsize(const std::string& s, size_t maxChars) {
    size_t count = 0, i = 0;
    while (i < s.size()) {
        unsigned char c = static_cast<unsigned char>(s[i]);
        size_t len = c < 0x80 ? 1 : (c >> 5) == 6 ? 2 : (c >> 4) == 14 ? 3 : 4;
        if (++count > maxChars) return s.substr(0, i) + "…";
        i += len;
    }
    return s;
}

size_t utf8Len(const std::string& s) {
    size_t n = 0;
    for (unsigned char c : s) if ((c & 0xC0) != 0x80) ++n;
    return n;
}

std::string trim(const std::string& s) {
    size_t a = s.find_first_not_of(" \t\r\n"), b = s.find_last_not_of(" \t\r\n");
    return a == std::string::npos ? "" : s.substr(a, b - a + 1);
}

std::string num(int v) {
    std::string s = std::to_string(v);
    if (v >= 1000000) s = std::to_string(v / 1000000) + "." + std::to_string((v / 100000) % 10) + "M";
    else if (v >= 10000) s = std::to_string(v / 1000) + "K";
    return fa() ? SocialService::persianDigits(s) : s;
}

std::string when(const std::string& iso, const std::string& style = "relative") {
    return SocialService::formatLocal(SocialService::parseIsoUtc(iso), style, fa(), nowSec());
}

Node* rowNode(float w, float h) {
    auto n = Node::create();
    n->setContentSize(Size(w, h));
    n->setAnchorPoint(Vec2(0.5f, 0.5f));
    return n;
}

Label* textAt(Node* parent, const std::string& s, float size, const Color3B& c, Weight wt, float x, float y, bool start = true,
              float maxW = 0) {
    auto l = makeText(s, size, c, wt, maxW, start ? TextHAlignment::LEFT : TextHAlignment::RIGHT);
    // "start" = reading start (left in LTR, right in RTL)
    float ax = start ? (fa() ? 1.f : 0.f) : (fa() ? 0.f : 1.f);
    l->setAnchorPoint(Vec2(ax, 0.5f));
    l->setPosition(x, y);
    parent->addChild(l);
    return l;
}

std::string followLabel(FollowState st, bool followsMe) {
    switch (st) {
        case FollowState::Following: return tx("دنبال می‌کنی", "Following");
        case FollowState::Requested: return tx("درخواست شد", "Requested");
        case FollowState::Self: return tx("ویرایش", "Edit");
        default: return followsMe ? tx("دنبال کردن متقابل", "Follow back") : tx("دنبال کردن", "Follow");
    }
}

ButtonStyle followStyle(FollowState st) {
    return st == FollowState::None ? ButtonStyle::Primary : ButtonStyle::Secondary;
}

/// A user row used by follower lists, search results and requests.
Node* userRow(const UserCard& u, float w, Node* clip, std::function<void(const UserCard&)> onChanged) {
    auto row = rowNode(w, 112);
    auto bg = makePanel(Size(w, 112), Color4F(1, 1, 1, 0.04f), color::f4(color::accent2, 0.25f), 22, 1);
    row->addChild(bg);
    auto av = Avatar::create(u.avatarUrl, u.name(), 80);
    av->setPosition(mx(62, w), 56);
    row->addChild(av);
    float tx0 = mx(118, w);
    textAt(row, ellipsize(u.name(), 22), 26, color::text, Weight::Bold, tx0, 72, true);
    textAt(row, "@" + u.username + (u.isPrivate ? tx("  · خصوصی", "  · private") : ""), 20, color::muted, Weight::Regular, tx0, 38, true);
    onTap(row, [id = u.id] { NxApp::push(ProfileScene::create(id)); }, clip);
    if (!u.isMe) {
        auto card = std::make_shared<UserCard>(u);
        SocialProfile sp;
        sp.user = u;
        FollowState st = sp.followState();
        NxButton* btn = nullptr;
        btn = NxButton::create(followLabel(st, false), Size(200, 62), [card, onChanged, row]() {
            auto self = static_cast<NxButton*>(row->getChildByName("follow"));
            SocialProfile cur;
            cur.user = *card;
            bool follow = cur.followState() == FollowState::None;
            auto doIt = [card, follow, onChanged, row]() {
                SocialProfile cur2;
                cur2.user = *card;
                SocialProfile opt = SocialService::optimisticFollow(cur2, follow);
                UserCard before = *card;
                *card = opt.user;
                if (auto b = static_cast<NxButton*>(row->getChildByName("follow"))) b->setTitle(followLabel(opt.followState(), false));
                row->retain();
                svc().setFollow(card->id, follow, [card, before, row, onChanged](const SbResult& r, const SocialProfile& p) {
                    if (r.ok) {
                        card->isFollowing = p.user.isFollowing;
                        card->isPending = p.user.isPending;
                    } else {
                        *card = before;  // roll back
                        toast(SocialService::friendlyError(r, fa()), true);
                    }
                    SocialProfile now;
                    now.user = *card;
                    if (auto b = static_cast<NxButton*>(row->getChildByName("follow"))) b->setTitle(followLabel(now.followState(), false));
                    if (onChanged) onChanged(*card);
                    row->release();
                });
            };
            if (follow) doIt();
            else
                showConfirm(cur.followState() == FollowState::Requested ? tx("درخواست لغو شود؟", "Cancel the follow request?")
                                                                        : tx("دنبال نکردن @", "Unfollow @") + card->username + "?",
                            cur.followState() == FollowState::Requested ? tx("لغو درخواست", "Cancel request") : tx("دنبال نکردن", "Unfollow"),
                            doIt);
            (void)self;
        }, followStyle(st));
        btn->setName("follow");
        btn->setPosition(mx(w - 118, w), 56);
        row->addChild(btn, 2);
    }
    return row;
}

Node* centeredMessage(float w, const std::string& title, const std::string& sub, float h = 260) {
    auto n = rowNode(w, h);
    auto t = makeText(title, 30, color::text, Weight::Bold, w - 80);
    t->setPosition(w / 2, h / 2 + 30);
    n->addChild(t);
    if (!sub.empty()) {
        auto s = makeText(sub, 22, color::muted, Weight::Regular, w - 100);
        s->setPosition(w / 2, h / 2 - 30);
        n->addChild(s);
    }
    return n;
}

}  // namespace

// =====================================================================================================================
// Shared

void showConfirm(const std::string& title, const std::string& confirmLabel, std::function<void()> onConfirm, bool danger) {
    auto scene = Director::getInstance()->getRunningScene();
    if (!scene) return;
    Size vs = visibleSize();
    Vec2 o = visibleOrigin();
    auto layer = LayerColor::create(Color4B(0, 0, 0, 150), vs.width + o.x * 2, vs.height + o.y * 2);
    auto block = EventListenerTouchOneByOne::create();
    block->setSwallowTouches(true);
    block->onTouchBegan = [](Touch*, Event*) { return true; };
    layer->getEventDispatcher()->addEventListenerWithSceneGraphPriority(block, layer);
    float w = vs.width - 80, h = 330;
    auto panel = makePanel(Size(w, h), color::f4(color::bgMid, 0.98f), color::f4(color::accent2, 0.8f), 30, 2);
    panel->setPosition(o.x + 40, o.y + vs.height / 2 - h / 2);
    layer->addChild(panel);
    auto t = makeText(title, 30, color::text, Weight::Bold, w - 60);
    t->setPosition(w / 2, h - 80);
    panel->addChild(t);
    auto ok = NxButton::create(confirmLabel, Size(w - 60, 80), [layer, onConfirm] {
        layer->removeFromParent();
        if (onConfirm) onConfirm();
    }, danger ? ButtonStyle::Danger : ButtonStyle::Primary);
    ok->setPosition(w / 2, 150);
    panel->addChild(ok);
    auto cancel = NxButton::create(tx("انصراف", "Cancel"), Size(w - 60, 70), [layer] { layer->removeFromParent(); }, ButtonStyle::Ghost);
    cancel->setPosition(w / 2, 60);
    panel->addChild(cancel);
    scene->addChild(layer, 20000);
}

void SocialSceneBase::buildBackground() {
    Size vs = visibleSize();
    Vec2 o = visibleOrigin();
    addChild(makeSpaceBackground(Size(vs.width + o.x * 2, vs.height + o.y * 2), false), -1);
}

Size SocialSceneBase::bodySize(float bottomReserve) const {
    Size vs = visibleSize();
    return Size(vs.width, vs.height - 116 - bottomReserve);
}

void SocialSceneBase::showLoading(VList* list, int rows) {
    list->clearRows();
    float w = list->getContentSize().width - 40;
    for (int i = 0; i < rows; ++i) {
        auto r = rowNode(w, 112);
        auto sk = makeSkeleton(Size(w, 112), 22);
        sk->setPosition(w / 2, 56);
        r->addChild(sk);
        list->addRow(r);
    }
}

void SocialSceneBase::showError(VList* list, const std::string& msg, std::function<void()> retry) {
    list->clearRows();
    float w = list->getContentSize().width - 40;
    list->addRow(centeredMessage(w, tx("بارگذاری نشد", "Couldn't load"), msg));
    auto r = rowNode(w, 100);
    auto b = NxButton::create(tx("تلاش دوباره", "Try again"), Size(300, 80), retry, ButtonStyle::Secondary);
    b->setPosition(w / 2, 50);
    r->addChild(b);
    list->addRow(r);
}

void SocialSceneBase::showEmpty(VList* list, const std::string& title, const std::string& sub) {
    list->clearRows();
    list->addRow(centeredMessage(list->getContentSize().width - 40, title, sub));
}

// =====================================================================================================================
// Profile

ProfileScene* ProfileScene::create(const std::string& userId) {
    auto s = new (std::nothrow) ProfileScene();
    if (s && s->initWith(userId)) {
        s->autorelease();
        return s;
    }
    delete s;
    return nullptr;
}

bool ProfileScene::initWith(const std::string& userId) {
    if (!Scene::init()) return false;
    userId_ = userId.empty() ? svc().myId() : userId;
    buildBackground();
    bar_ = makeTopBar(tx("پروفایل", "Profile"));
    addChild(bar_, 5);
    Size b = bodySize();
    list_ = VList::create(b, 16);
    list_->setPosition(visibleOrigin());
    addChild(list_);
    showLoading(list_, 5);
    return true;
}

void ProfileScene::onEnter() {
    SocialSceneBase::onEnter();
    load();  // fresh counters every time the screen shows (e.g. after editing or following from a list)
    firstEnter_ = false;
}

void ProfileScene::load() {
    auto alive = this->alive();
    svc().profile(userId_, [this, alive](const SbResult& r, const SocialProfile& p) {
        if (!*alive) return;
        if (!r.ok) {
            if (!p_.loaded) showError(list_, SocialService::friendlyError(r, fa()), [this] { showLoading(list_, 5); load(); });
            return;
        }
        p_ = p;
        render();
        if (p_.canView) {
            svc().userPosts(userId_, [this, alive](const SbResult& pr, const std::vector<SocialPost>& v) {
                if (!*alive) return;
                postsLoaded_ = true;
                postsError_ = !pr.ok;
                posts_ = v;
                render();
            });
        } else {
            postsLoaded_ = true;
            posts_.clear();
            render();
        }
    });
}

void ProfileScene::render() {
    if (!p_.loaded) return;
    list_->clearRows();
    float w = list_->getContentSize().width - 40;
    bool me = p_.user.isMe;

    // ---- header: avatar, names, bio, links
    float headerH = 300;
    if (!p_.bio.empty()) headerH += 30 + 34 * std::min<size_t>(4, 1 + utf8Len(p_.bio) / 34);
    if (!p_.website.empty() || !p_.location.empty()) headerH += 40;
    auto head = rowNode(w, headerH);
    float y = headerH - 100;
    auto av = Avatar::create(p_.user.avatarUrl, p_.user.name(), 180);
    av->setPosition(w / 2, y);
    head->addChild(av);
    y -= 120;
    auto name = makeText(ellipsize(p_.user.name(), 28), 36, color::text, Weight::Black, w - 40);
    name->setPosition(w / 2, y);
    head->addChild(name);
    y -= 44;
    auto un = makeText("@" + p_.user.username + (p_.user.isPrivate ? tx("  ·  خصوصی", "  ·  private") : "") + (p_.followsMe && !me ? tx("  ·  تو را دنبال می‌کند", "  ·  follows you") : ""), 22,
                       color::muted, Weight::Medium, w - 40);
    un->setPosition(w / 2, y);
    head->addChild(un);
    y -= 30;
    if (!p_.bio.empty()) {
        auto bio = makeText(p_.bio, 24, color::text, Weight::Regular, w - 80);
        bio->setAnchorPoint(Vec2(0.5f, 1.f));
        bio->setPosition(w / 2, y - 10);
        head->addChild(bio);
        y -= 30 + bio->getContentSize().height;
    }
    if (!p_.website.empty() || !p_.location.empty()) {
        std::string line;
        if (!p_.location.empty()) line += p_.location;
        if (!p_.website.empty()) line += (line.empty() ? "" : "   ") + p_.website;
        auto l = makeText(ellipsize(line, 48), 22, color::accent, Weight::Medium, w - 40);
        l->setPosition(w / 2, y - 18);
        head->addChild(l);
        if (!p_.website.empty()) {
            std::string url = p_.website.rfind("http", 0) == 0 ? p_.website : "https://" + p_.website;
            onTap(l, [url] { native::openUrl(url); }, list_);
        }
    }
    list_->addRow(head);

    // ---- counters
    auto stats = rowNode(w, 130);
    auto sp = makePanel(Size(w, 130), Color4F(1, 1, 1, 0.05f), color::f4(color::accent2, 0.35f), 26, 1);
    stats->addChild(sp);
    struct Stat { std::string n, label, kind; };
    std::vector<Stat> st = {{num(p_.posts), tx("پست", "Posts"), ""},
                            {num(p_.followers), tx("دنبال‌کننده", "Followers"), "followers"},
                            {num(p_.following), tx("دنبال‌شونده", "Following"), "following"}};
    for (int i = 0; i < 3; ++i) {
        float cx = mx(w / 6 + i * w / 3, w);
        auto cell = rowNode(w / 3, 130);
        cell->setPosition(cx, 65);
        stats->addChild(cell);
        auto nl = makeText(st[i].n, 34, color::text, Weight::Black);
        nl->setPosition(w / 6, 82);
        cell->addChild(nl);
        auto ll = makeText(st[i].label, 20, color::muted, Weight::Medium);
        ll->setPosition(w / 6, 40);
        cell->addChild(ll);
        if (!st[i].kind.empty()) {
            std::string kind = st[i].kind, uid = userId_, label = st[i].label;
            bool can = p_.canView;
            onTap(cell, [kind, uid, label, can] {
                if (!can) { toast(tx("این حساب خصوصی است.", "This account is private."), true); return; }
                NxApp::push(FollowListScene::create(uid, kind, label));
            }, list_);
        }
    }
    list_->addRow(stats);

    // ---- actions
    auto actions = rowNode(w, 96);
    float bw = (w - 20) / 2;
    if (me) {
        auto edit = NxButton::create(tx("ویرایش پروفایل", "Edit profile"), Size(bw, 80),
                                     [this] { NxApp::push(EditProfileScene::create(p_)); }, ButtonStyle::Secondary);
        edit->setPosition(mx(bw / 2, w), 48);
        actions->addChild(edit);
        std::string reqLabel = tx("درخواست‌ها", "Requests") + (p_.pendingRequests > 0 ? " (" + num(p_.pendingRequests) + ")" : "");
        auto req = NxButton::create(reqLabel, Size(bw, 80),
                                    [] { NxApp::push(FollowListScene::create("", "requests", tx("درخواست‌های دنبال کردن", "Follow requests"))); },
                                    p_.pendingRequests > 0 ? ButtonStyle::Primary : ButtonStyle::Ghost);
        req->setPosition(mx(w - bw / 2, w), 48);
        actions->addChild(req);
    } else {
        FollowState fs = p_.followState();
        auto follow = NxButton::create(followLabel(fs, p_.followsMe), Size(bw, 80), [this] { tapFollow(); }, followStyle(fs));
        follow->setPosition(mx(bw / 2, w), 48);
        follow->setEnabled(!busy_ && !p_.isBlocked);
        actions->addChild(follow);
        std::string uid = userId_;
        auto msg = NxButton::create(tx("پیام", "Message"), Size(bw, 80), [uid] { NxApp::push(ChatScene::create(uid)); },
                                    ButtonStyle::Secondary);
        msg->setPosition(mx(w - bw / 2, w), 48);
        msg->setEnabled(!p_.isBlocked);
        actions->addChild(msg);
    }
    list_->addRow(actions);
    renderPosts();
}

void ProfileScene::renderPosts() {
    float w = list_->getContentSize().width - 40;
    if (!p_.canView) {
        list_->addRow(centeredMessage(w, tx("این حساب خصوصی است", "This account is private"),
                                      tx("برای دیدن پست‌ها و دنبال‌کننده‌ها، دنبالش کن.", "Follow to see their posts and followers.")));
        return;
    }
    if (!postsLoaded_) {
        auto r = rowNode(w, 240);
        float t = (w - 16) / 3;
        for (int i = 0; i < 3; ++i) {
            auto sk = makeSkeleton(Size(t, t), 14);
            sk->setPosition(t / 2 + i * (t + 8), 120);
            r->addChild(sk);
        }
        list_->addRow(r);
        return;
    }
    if (postsError_) {
        list_->addRow(centeredMessage(w, tx("پست‌ها بارگذاری نشد", "Couldn't load posts"), tx("صفحه را دوباره باز کن.", "Reopen this page."), 160));
        return;
    }
    if (posts_.empty()) {
        list_->addRow(centeredMessage(w, tx("هنوز پستی نیست", "No posts yet"),
                                      p_.user.isMe ? tx("اولین پستت را در نوا گالری منتشر کن.", "Share your first post in Nova Gallery.") : "", 200));
        return;
    }
    float t = (w - 16) / 3;
    for (size_t i = 0; i < posts_.size(); i += 3) {
        auto r = rowNode(w, t);
        for (size_t k = i; k < std::min(i + 3, posts_.size()); ++k) {
            const SocialPost& post = posts_[k];
            int col = static_cast<int>(k - i);
            float cx = mx(t / 2 + col * (t + 8), w);
            Node* tile;
            if (!post.imageUrl.empty()) {
                tile = ImageBox::create(post.imageUrl, Size(t, t), 14);
            } else {
                tile = rowNode(t, t);
                auto bg = makePanel(Size(t, t), color::f4(color::accent2, 0.18f), color::f4(color::accent2, 0.5f), 14, 1);
                tile->addChild(bg);
                auto tl = makeText(ellipsize(post.text, 60), 18, color::text, Weight::Medium, t - 24);
                tl->setPosition(t / 2, t / 2 + 12);
                tile->addChild(tl);
            }
            auto date = makeText(when(post.createdAt, "date"), 15, color::muted);
            date->setPosition(t / 2, 16);
            tile->addChild(date, 3);
            tile->setPosition(cx, t / 2);
            r->addChild(tile);
            SocialPost cp = post;
            UserCard author = p_.user;
            onTap(tile, [cp, author] { NxApp::push(PostScene::create(cp, author)); }, list_);
        }
        list_->addRow(r);
    }
}

void ProfileScene::tapFollow() {
    if (busy_) return;
    FollowState st = p_.followState();
    if (st == FollowState::None) { applyFollow(true); return; }
    showConfirm(st == FollowState::Requested ? tx("درخواست دنبال کردن لغو شود؟", "Cancel your follow request?")
                                             : tx("دنبال نکردن @", "Unfollow @") + p_.user.username + "?",
                st == FollowState::Requested ? tx("لغو درخواست", "Cancel request") : tx("دنبال نکردن", "Unfollow"),
                [this] { applyFollow(false); });
}

void ProfileScene::applyFollow(bool follow) {
    SocialProfile before = p_;
    p_ = SocialService::optimisticFollow(p_, follow);  // instant UI
    busy_ = true;
    render();
    auto alive = this->alive();
    svc().setFollow(userId_, follow, [this, alive, before](const SbResult& r, const SocialProfile& server) {
        if (!*alive) return;
        busy_ = false;
        if (r.ok) {
            bool hadAccess = p_.canView;
            p_ = server;
            if (p_.canView && !hadAccess) { postsLoaded_ = false; load(); return; }
        } else {
            p_ = before;  // roll back to the previous state
            toast(SocialService::friendlyError(r, fa()), true);
        }
        render();
    });
}

// =====================================================================================================================
// Edit profile

EditProfileScene* EditProfileScene::create(const SocialProfile& p) {
    auto s = new (std::nothrow) EditProfileScene();
    if (s && s->initWith(p)) {
        s->autorelease();
        return s;
    }
    delete s;
    return nullptr;
}

bool EditProfileScene::initWith(const SocialProfile& p) {
    if (!Scene::init()) return false;
    p_ = p;
    buildBackground();
    addChild(makeTopBar(tx("ویرایش پروفایل", "Edit profile")), 5);
    build();
    return true;
}

void EditProfileScene::build() {
    Size b = bodySize();
    auto list = VList::create(b, 18);
    list->setPosition(visibleOrigin());
    addChild(list);
    float w = b.width - 40;

    auto avRow = rowNode(w, 300);
    avatar_ = Avatar::create(p_.user.avatarUrl, p_.user.name(), 190);
    avatar_->setPosition(w / 2, 190);
    avRow->addChild(avatar_);
    float bw = (w - 20) / 2;
    photoBtn_ = NxButton::create(tx("تغییر عکس", "Change photo"), Size(bw, 70), [this] { pickAvatar(); }, ButtonStyle::Secondary);
    photoBtn_->setPosition(mx(bw / 2, w), 40);
    avRow->addChild(photoBtn_);
    auto rm = NxButton::create(tx("حذف عکس", "Remove photo"), Size(bw, 70), [this] {
        if (p_.user.avatarUrl.empty()) return;
        showConfirm(tx("عکس پروفایل حذف شود؟", "Remove your profile photo?"), tx("حذف", "Remove"), [this] { removeAvatar(); });
    }, ButtonStyle::Ghost);
    rm->setPosition(mx(w - bw / 2, w), 40);
    avRow->addChild(rm);
    list->addRow(avRow);

    auto field = [&](const std::string& label, const std::string& value, cocos2d::ui::EditBox::InputMode mode, int maxLen) {
        auto r = rowNode(w, 136);
        textAt(r, label, 22, color::muted, Weight::Medium, mx(10, w), 116, true);
        auto e = makeInput(Size(w, 84), label, false, mode);
        e->setText(value.c_str());
        e->setMaxLength(maxLen);
        e->setPosition(Vec2(w / 2, 50));
        r->addChild(e);
        list->addRow(r);
        return e;
    };
    name_ = field(tx("نام", "Name"), p_.user.displayName, cocos2d::ui::EditBox::InputMode::SINGLE_LINE, 40);
    username_ = field(tx("نام کاربری", "Username"), p_.user.username, cocos2d::ui::EditBox::InputMode::SINGLE_LINE, 20);
    bio_ = field(tx("بیو (حداکثر ۱۶۰ حرف)", "Bio (max 160)"), p_.bio, cocos2d::ui::EditBox::InputMode::ANY, 160);
    website_ = field(tx("وب‌سایت", "Website"), p_.website, cocos2d::ui::EditBox::InputMode::URL, 120);
    location_ = field(tx("محل زندگی", "Location"), p_.location, cocos2d::ui::EditBox::InputMode::SINGLE_LINE, 60);

    auto privRow = rowNode(w, 100);
    auto pp = makePanel(Size(w, 100), Color4F(1, 1, 1, 0.05f), color::f4(color::accent2, 0.4f), 22, 1);
    privRow->addChild(pp);
    textAt(privRow, tx("حساب خصوصی", "Private account"), 26, color::text, Weight::Medium, mx(26, w), 62, true);
    textAt(privRow, tx("فقط دنبال‌کننده‌های تأییدشده پست‌هایت را می‌بینند", "Only approved followers see your posts"), 18,
           color::muted, Weight::Regular, mx(26, w), 28, true);
    privBtn_ = NxButton::create(p_.user.isPrivate ? tx("روشن", "On") : tx("خاموش", "Off"), Size(140, 62), [this] { togglePrivate(); },
                                p_.user.isPrivate ? ButtonStyle::Primary : ButtonStyle::Ghost);
    privBtn_->setPosition(mx(w - 90, w), 50);
    privRow->addChild(privBtn_);
    list->addRow(privRow);

    auto act = rowNode(w, 200);
    saveBtn_ = NxButton::create(tx("ذخیره", "Save"), Size(w, 86), [this] { save(); });
    saveBtn_->setPosition(w / 2, 150);
    act->addChild(saveBtn_);
    auto pw = NxButton::create(tx("تغییر رمز عبور", "Change password"), Size(w, 76),
                               [] { NxApp::push(ChangePasswordScene::create(false)); }, ButtonStyle::Ghost);
    pw->setPosition(w / 2, 50);
    act->addChild(pw);
    list->addRow(act);
}

void EditProfileScene::pickAvatar() {
    if (busy_) return;
    auto alive = this->alive();
    native::pickImage([this, alive](const std::string& path) {
        if (!*alive) return;
        if (path.empty()) return;  // cancelled
        if (path.rfind("error:", 0) == 0) {
            toast(path == "error:type" ? tx("فقط عکس JPG، PNG یا WEBP.", "Only JPG, PNG or WEBP images.")
                                       : tx("خواندن عکس ممکن نشد.", "Couldn't read that image."), true);
            return;
        }
        Data d = FileUtils::getInstance()->getDataFromFile(path);
        std::string bytes(reinterpret_cast<const char*>(d.getBytes()), static_cast<size_t>(d.getSize()));
        avatar_->setLocalFile(path);  // instant preview
        busy_ = true;
        photoBtn_->setTitle(tx("در حال آپلود…", "Uploading…"));
        photoBtn_->setEnabled(false);
        svc().uploadAvatar(bytes, [this, alive, path](const SbResult& r, const SocialProfile& p) {
            FileUtils::getInstance()->removeFile(path);
            if (!*alive) return;
            busy_ = false;
            photoBtn_->setTitle(tx("تغییر عکس", "Change photo"));
            photoBtn_->setEnabled(true);
            if (!r.ok) {
                avatar_->setUrl(p_.user.avatarUrl, p_.user.name());
                toast(SocialService::friendlyError(r, fa()), true);
                return;
            }
            p_.user.avatarUrl = p.user.avatarUrl;
            avatar_->setUrl(p_.user.avatarUrl, p_.user.name());
            toast(tx("عکس پروفایل ذخیره شد.", "Profile photo saved."));
        });
    });
}

void EditProfileScene::removeAvatar() {
    auto alive = this->alive();
    busy_ = true;
    svc().removeAvatar([this, alive](const SbResult& r, const SocialProfile&) {
        if (!*alive) return;
        busy_ = false;
        if (!r.ok) { toast(SocialService::friendlyError(r, fa()), true); return; }
        p_.user.avatarUrl.clear();
        avatar_->setUrl("", p_.user.name());
    });
}

void EditProfileScene::togglePrivate() {
    if (busy_) return;
    bool target = !p_.user.isPrivate;
    auto alive = this->alive();
    auto run = [this, alive, target] {
        busy_ = true;
        svc().setPrivacy(target, [this, alive](const SbResult& r, const SocialProfile& p) {
            if (!*alive) return;
            busy_ = false;
            if (!r.ok) { toast(SocialService::friendlyError(r, fa()), true); return; }
            p_.user.isPrivate = p.user.isPrivate;
            privBtn_->setTitle(p_.user.isPrivate ? tx("روشن", "On") : tx("خاموش", "Off"));
        });
    };
    if (!target)
        showConfirm(tx("حساب عمومی شود؟ درخواست‌های در انتظار پذیرفته می‌شوند.", "Make your account public? Pending requests will be accepted."),
                    tx("عمومی کن", "Make public"), run, false);
    else run();
}

void EditProfileScene::save() {
    if (busy_) return;
    std::string name = trim(name_->getText()), user = trim(username_->getText()), bio = trim(bio_->getText()),
                web = trim(website_->getText()), loc = trim(location_->getText());
    for (auto& c : user) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    if (!std::regex_match(user, std::regex("^[a-z0-9_.]{3,20}$"))) {
        toast(tx("نام کاربری ۳ تا ۲۰ حرف انگلیسی کوچک، عدد، _ یا . باشد.", "Username: 3–20 lowercase letters, digits, _ or ."), true);
        return;
    }
    if (utf8Len(bio) > 160) { toast(tx("بیو حداکثر ۱۶۰ حرف است.", "Bio can be at most 160 characters."), true); return; }
    busy_ = true;
    saveBtn_->setEnabled(false);
    saveBtn_->setTitle(tx("در حال ذخیره…", "Saving…"));
    auto alive = this->alive();
    svc().saveProfile(name, user, bio, web, loc, [this, alive](const SbResult& r, const SocialProfile& p) {
        if (!*alive) return;
        busy_ = false;
        saveBtn_->setEnabled(true);
        saveBtn_->setTitle(tx("ذخیره", "Save"));
        if (!r.ok) { toast(SocialService::friendlyError(r, fa()), true); return; }
        p_ = p;
        NxApp::get().setPlayerName(p.user.name());
        toast(tx("پروفایل ذخیره شد.", "Profile saved."));
        NxApp::pop();
    });
}

// =====================================================================================================================
// Followers / following / requests

FollowListScene* FollowListScene::create(const std::string& userId, const std::string& kind, const std::string& title) {
    auto s = new (std::nothrow) FollowListScene();
    if (s && s->initWith(userId, kind, title)) {
        s->autorelease();
        return s;
    }
    delete s;
    return nullptr;
}

bool FollowListScene::initWith(const std::string& userId, const std::string& kind, const std::string& title) {
    if (!Scene::init()) return false;
    userId_ = userId;
    kind_ = kind;
    buildBackground();
    addChild(makeTopBar(title), 5);
    list_ = VList::create(bodySize(), 12);
    list_->setPosition(visibleOrigin());
    addChild(list_);
    showLoading(list_, 6);
    return true;
}

void FollowListScene::onEnter() {
    SocialSceneBase::onEnter();
    rows_.clear();
    load(false);
    firstEnter_ = false;
}

void FollowListScene::load(bool more) {
    if (loading_) return;
    loading_ = true;
    auto alive = this->alive();
    auto done = [this, alive, more](const SbResult& r, const std::vector<UserCard>& v) {
        if (!*alive) return;
        loading_ = false;
        if (!r.ok) {
            if (rows_.empty()) showError(list_, SocialService::friendlyError(r, fa()), [this] { showLoading(list_, 6); load(false); });
            else toast(SocialService::friendlyError(r, fa()), true);
            return;
        }
        if (!more) rows_.clear();
        rows_.insert(rows_.end(), v.begin(), v.end());
        hasMore_ = kind_ != "requests" && v.size() >= 50;
        render();
    };
    if (kind_ == "requests") svc().followRequests(done);
    else svc().followList(userId_, kind_, more ? static_cast<int>(rows_.size()) : 0, done);
}

void FollowListScene::render() {
    list_->clearRows();
    float w = list_->getContentSize().width - 40;
    if (rows_.empty()) {
        if (kind_ == "requests") showEmpty(list_, tx("درخواستی نداری", "No follow requests"), "");
        else if (kind_ == "followers") showEmpty(list_, tx("هنوز دنبال‌کننده‌ای نیست", "No followers yet"), "");
        else showEmpty(list_, tx("هنوز کسی را دنبال نمی‌کند", "Not following anyone yet"), "");
        return;
    }
    for (size_t i = 0; i < rows_.size(); ++i) {
        const UserCard& u = rows_[i];
        if (kind_ == "requests") {
            auto row = rowNode(w, 112);
            row->addChild(makePanel(Size(w, 112), Color4F(1, 1, 1, 0.04f), color::f4(color::accent2, 0.25f), 22, 1));
            auto av = Avatar::create(u.avatarUrl, u.name(), 80);
            av->setPosition(mx(62, w), 56);
            row->addChild(av);
            textAt(row, ellipsize(u.name(), 14), 24, color::text, Weight::Bold, mx(118, w), 72, true);
            textAt(row, "@" + ellipsize(u.username, 14), 19, color::muted, Weight::Regular, mx(118, w), 38, true);
            onTap(row, [id = u.id] { NxApp::push(ProfileScene::create(id)); }, list_);
            auto alive = this->alive();
            std::string id = u.id;
            auto respond = [this, alive, id](bool accept) {
                svc().respondRequest(id, accept, [this, alive, id, accept](const SbResult& r) {
                    if (!*alive) return;
                    if (!r.ok) { toast(SocialService::friendlyError(r, fa()), true); return; }
                    rows_.erase(std::remove_if(rows_.begin(), rows_.end(), [&](const UserCard& c) { return c.id == id; }), rows_.end());
                    toast(accept ? tx("درخواست پذیرفته شد.", "Request accepted.") : tx("درخواست رد شد.", "Request declined."));
                    render();
                });
            };
            auto ok = NxButton::create(tx("تأیید", "Accept"), Size(130, 60), [respond] { respond(true); });
            ok->setPosition(mx(w - 220, w), 56);
            row->addChild(ok, 2);
            auto no = NxButton::create(tx("رد", "Decline"), Size(130, 60), [respond] { respond(false); }, ButtonStyle::Ghost);
            no->setPosition(mx(w - 78, w), 56);
            row->addChild(no, 2);
            list_->addRow(row);
        } else {
            size_t idx = i;
            list_->addRow(userRow(u, w, list_, [this, idx](const UserCard& c) { if (idx < rows_.size()) rows_[idx] = c; }));
        }
    }
    if (hasMore_) {
        auto r = rowNode(w, 100);
        auto b = NxButton::create(tx("بیشتر", "Load more"), Size(300, 76), [this] { load(true); }, ButtonStyle::Ghost);
        b->setPosition(w / 2, 50);
        r->addChild(b);
        list_->addRow(r);
    }
}

// =====================================================================================================================
// Social home: search / inbox / alerts

Scene* SocialHomeScene::createAt(const std::string& tab) {
    if (tab == "profile") return ProfileScene::create(NxApp::get().social().myId());
    auto s = new (std::nothrow) SocialHomeScene();
    if (s && s->initWith(tab)) {
        s->autorelease();
        return s;
    }
    delete s;
    return nullptr;
}

bool SocialHomeScene::initWith(const std::string& tab) {
    if (!Scene::init()) return false;
    buildBackground();
    Size vs = visibleSize();
    Vec2 o = visibleOrigin();
    auto bar = makeTopBar(tx("شبکه‌ی اجتماعی", "Social"));
    addChild(bar, 5);
    // my avatar (opens my profile) in the top bar
    auto me = Avatar::create("", NxApp::get().playerName(), 70);
    me->setPosition(fa() ? 66 : vs.width - 66, 54);
    me->setName("meAvatar");
    bar->addChild(me, 3);
    onTap(me, [] { NxApp::push(ProfileScene::create(NxApp::get().social().myId())); });
    auto alive = this->alive();
    svc().profile(svc().myId(), [me, alive](const SbResult& r, const SocialProfile& p) {
        if (*alive && r.ok) me->setUrl(p.user.avatarUrl, p.user.name());
    });
    content_ = Node::create();
    content_->setPosition(o);
    addChild(content_, 1);
    buildTabBar();
    selectTab(tab.empty() ? "search" : tab);
    return true;
}

void SocialHomeScene::onEnter() {
    SocialSceneBase::onEnter();
    refreshBadges();
    if (tab_ == "inbox") renderInbox();
    schedule([this](float) { refreshBadges(); }, 20.f, "nx.social.badges");
}

void SocialHomeScene::onExit() {
    unschedule("nx.social.badges");
    unschedule("nx.social.search");
    SocialSceneBase::onExit();
}

void SocialHomeScene::buildTabBar() {
    if (tabBar_) tabBar_->removeFromParent();
    Size vs = visibleSize();
    Vec2 o = visibleOrigin();
    tabBar_ = Node::create();
    tabBar_->setPosition(o);
    addChild(tabBar_, 6);
    auto bg = makePanel(Size(vs.width - 24, 104), color::f4(color::bgMid, 0.95f), color::f4(color::accent2, 0.6f), 30, 1.5f);
    bg->setPosition(12, 10);
    tabBar_->addChild(bg);
    struct T { std::string id, label; int badge; };
    std::vector<T> tabs = {{"search", tx("جستجو", "Search"), 0},
                           {"inbox", tx("پیام‌ها", "Messages"), unreadChats_},
                           {"alerts", tx("اعلان‌ها", "Alerts"), unreadAlerts_}};
    float tw = (vs.width - 48) / 3;
    for (int i = 0; i < 3; ++i) {
        std::string label = tabs[i].label + (tabs[i].badge > 0 ? " (" + num(tabs[i].badge) + ")" : "");
        std::string id = tabs[i].id;
        auto b = NxButton::create(label, Size(tw - 8, 80), [this, id] { selectTab(id); },
                                  id == tab_ ? ButtonStyle::Primary : ButtonStyle::Ghost);
        b->setPosition(mx(24 + tw / 2 + i * tw, vs.width), 62);
        tabBar_->addChild(b);
    }
}

void SocialHomeScene::selectTab(const std::string& tab) {
    tab_ = tab;
    content_->removeAllChildren();
    searchBox_ = nullptr;
    unschedule("nx.social.search");
    buildTabBar();
    if (tab == "inbox") renderInbox();
    else if (tab == "alerts") renderAlerts();
    else renderSearch();
}

void SocialHomeScene::refreshBadges() {
    auto alive = this->alive();
    svc().notifications([this, alive](const SbResult& r, const std::vector<SocialNotification>& v, int unread) {
        if (!*alive || !r.ok) return;
        int alerts = 0, chats = 0;
        for (auto& n : v)
            if (!n.read) (n.kind == "message" ? chats : alerts)++;
        (void)unread;
        if (alerts != unreadAlerts_ || chats != unreadChats_) {
            unreadAlerts_ = alerts;
            unreadChats_ = chats;
            buildTabBar();
        }
    });
}

void SocialHomeScene::renderSearch() {
    Size b = bodySize(124);
    float w = b.width - 40;
    searchBox_ = makeInput(Size(w, 84), tx("جستجوی نام یا نام کاربری", "Search name or username"));
    searchBox_->setPosition(Vec2(b.width / 2, 124 + b.height - 50));
    content_->addChild(searchBox_, 2);
    list_ = VList::create(Size(b.width, b.height - 110), 12);
    list_->setPosition(Vec2(0, 124));
    content_->addChild(list_);
    showEmpty(list_, tx("کاوشگرها را پیدا کن", "Find explorers"), tx("نام یا @نام‌کاربری را بنویس.", "Type a name or @username."));
    lastQuery_.clear();
    // Debounced live search: poll the box text and query 0.4 s after typing stops.
    auto pending = std::make_shared<std::pair<std::string, int>>("", 0);
    schedule([this, pending](float) {
        if (!searchBox_) return;
        std::string q = trim(searchBox_->getText());
        if (!q.empty() && q[0] == '@') q = q.substr(1);
        if (q == lastQuery_) return;
        if (q != pending->first) { pending->first = q; pending->second = 0; return; }
        if (++pending->second < 2) return;
        lastQuery_ = q;
        runSearch(q);
    }, 0.2f, "nx.social.search");
}

void SocialHomeScene::runSearch(const std::string& q) {
    if (q.empty()) {
        showEmpty(list_, tx("کاوشگرها را پیدا کن", "Find explorers"), tx("نام یا @نام‌کاربری را بنویس.", "Type a name or @username."));
        return;
    }
    int seq = ++searchSeq_;
    showLoading(list_, 3);
    auto alive = this->alive();
    svc().search(q, [this, alive, seq](const SbResult& r, const std::vector<UserCard>& v) {
        if (!*alive || seq != searchSeq_ || tab_ != "search") return;
        if (!r.ok) { showError(list_, SocialService::friendlyError(r, fa()), [this] { runSearch(lastQuery_); }); return; }
        if (v.empty()) { showEmpty(list_, tx("کسی پیدا نشد", "No one found"), ""); return; }
        list_->clearRows();
        float w = list_->getContentSize().width - 40;
        for (auto& u : v) list_->addRow(userRow(u, w, list_, nullptr));
    });
}

void SocialHomeScene::renderInbox() {
    content_->removeAllChildren();
    Size b = bodySize(124);
    list_ = VList::create(b, 12);
    list_->setPosition(Vec2(0, 124));
    content_->addChild(list_);
    showLoading(list_, 5);
    auto alive = this->alive();
    svc().myChats([this, alive](const SbResult& r, const std::vector<ChatSummary>& v) {
        if (!*alive || tab_ != "inbox") return;
        if (!r.ok) { showError(list_, SocialService::friendlyError(r, fa()), [this] { renderInbox(); }); return; }
        list_->clearRows();
        float w = list_->getContentSize().width - 40;
        int shown = 0;
        for (auto& c : v) {
            if (c.kind != "direct" || c.peerId.empty()) continue;
            ++shown;
            auto row = rowNode(w, 120);
            row->addChild(makePanel(Size(w, 120), Color4F(1, 1, 1, c.unread > 0 ? 0.09f : 0.04f),
                                    color::f4(c.unread > 0 ? color::accent : color::accent2, 0.35f), 22, 1));
            auto av = Avatar::create(c.peerAvatar.rfind("http", 0) == 0 ? c.peerAvatar : "", c.peerName, 84);
            av->setPosition(mx(64, w), 60);
            row->addChild(av);
            textAt(row, ellipsize(c.peerName, 20), 26, color::text, Weight::Bold, mx(124, w), 80, true);
            textAt(row, ellipsize(c.lastText.empty() ? tx("گفتگو را شروع کن", "Start the conversation") : c.lastText, 30), 20,
                   c.unread > 0 ? color::text : color::muted, Weight::Regular, mx(124, w), 42, true);
            textAt(row, when(c.lastAt), 18, color::muted, Weight::Regular, mx(w - 24, w), 84, false);
            if (c.unread > 0) {
                auto badge = DrawNode::create();
                badge->drawSolidCircle(Vec2::ZERO, 20, 0, 24, color::f4(color::pink));
                badge->setPosition(mx(w - 44, w), 40);
                row->addChild(badge);
                auto bl = makeText(num(c.unread), 18, color::text, Weight::Bold);
                bl->setPosition(mx(w - 44, w), 40);
                row->addChild(bl);
            }
            std::string peer = c.peerId;
            onTap(row, [peer] { NxApp::push(ChatScene::create(peer)); }, list_);
            list_->addRow(row);
        }
        if (shown == 0)
            showEmpty(list_, tx("هنوز پیامی نداری", "No messages yet"),
                      tx("از صفحه‌ی پروفایل هر کاوشگر روی «پیام» بزن.", "Tap Message on anyone's profile to start a chat."));
    });
}

void SocialHomeScene::renderAlerts() {
    Size b = bodySize(124);
    list_ = VList::create(b, 12);
    list_->setPosition(Vec2(0, 124));
    content_->addChild(list_);
    showLoading(list_, 5);
    auto alive = this->alive();
    svc().profile(svc().myId(), [this, alive](const SbResult& pr, const SocialProfile& me) {
        if (!*alive || tab_ != "alerts") return;
        int pending = pr.ok ? me.pendingRequests : 0;
        svc().notifications([this, alive, pending](const SbResult& r, const std::vector<SocialNotification>& v, int) {
            if (!*alive || tab_ != "alerts") return;
            if (!r.ok) { showError(list_, SocialService::friendlyError(r, fa()), [this] { selectTab("alerts"); }); return; }
            list_->clearRows();
            float w = list_->getContentSize().width - 40;
            if (pending > 0) {
                auto row = rowNode(w, 96);
                row->addChild(makePanel(Size(w, 96), color::f4(color::accent2, 0.2f), color::f4(color::accent, 0.7f), 22, 1.5f));
                textAt(row, tx("درخواست‌های دنبال کردن", "Follow requests") + " (" + num(pending) + ")", 26, color::text, Weight::Bold,
                       mx(28, w), 48, true);
                onTap(row, [] { NxApp::push(FollowListScene::create("", "requests", tx("درخواست‌های دنبال کردن", "Follow requests"))); }, list_);
                list_->addRow(row);
            }
            int shown = 0;
            for (auto& n : v) {
                std::string text;
                std::string who = n.actor.name().empty() ? tx("یک کاوشگر", "Someone") : n.actor.name();
                if (n.kind == "follow") text = who + tx(" تو را دنبال کرد", " started following you");
                else if (n.kind == "follow_request") text = who + tx(" درخواست دنبال کردن فرستاد", " requested to follow you");
                else if (n.kind == "follow_accepted") text = who + tx(" درخواستت را پذیرفت", " accepted your follow request");
                else if (n.kind == "message") text = who + tx(" برایت پیام فرستاد", " sent you a message");
                else continue;
                ++shown;
                auto row = rowNode(w, 108);
                row->addChild(makePanel(Size(w, 108), Color4F(1, 1, 1, n.read ? 0.03f : 0.09f),
                                        color::f4(n.read ? color::accent2 : color::accent, 0.3f), 22, 1));
                auto av = Avatar::create(n.actor.avatarUrl, n.actor.name(), 76);
                av->setPosition(mx(60, w), 54);
                row->addChild(av);
                textAt(row, ellipsize(text, 40), 22, color::text, n.read ? Weight::Regular : Weight::Bold, mx(114, w), 66, true, w - 140);
                textAt(row, when(n.createdAt), 18, color::muted, Weight::Regular, mx(114, w), 30, true);
                std::string actor = n.actor.id, kind = n.kind;
                onTap(row, [actor, kind] {
                    if (actor.empty()) return;
                    if (kind == "message") NxApp::push(ChatScene::create(actor));
                    else NxApp::push(ProfileScene::create(actor));
                }, list_);
                list_->addRow(row);
            }
            if (shown == 0 && pending == 0) showEmpty(list_, tx("اعلانی نداری", "No notifications"), "");
            svc().markAllNotificationsRead();
            unreadAlerts_ = 0;
        });
    });
}

// =====================================================================================================================
// Direct chat

ChatScene* ChatScene::create(const std::string& peerId) {
    auto s = new (std::nothrow) ChatScene();
    if (s && s->initWith(peerId)) {
        s->autorelease();
        return s;
    }
    delete s;
    return nullptr;
}

bool ChatScene::initWith(const std::string& peerId) {
    if (!Scene::init()) return false;
    peerId_ = peerId;
    buildBackground();
    Size vs = visibleSize();
    Vec2 o = visibleOrigin();
    header_ = makeTopBar(" ");
    addChild(header_, 5);
    // input bar
    auto bar = Node::create();
    bar->setPosition(o);
    addChild(bar, 6);
    bar->addChild(makePanel(Size(vs.width, 120), color::f4(color::bgMid, 0.97f), color::f4(color::accent2, 0.5f), 0, 1));
    input_ = makeInput(Size(vs.width - 220, 84), tx("پیام…", "Message…"), false, cocos2d::ui::EditBox::InputMode::ANY);
    input_->setMaxLength(2000);
    input_->setPosition(Vec2(fa() ? vs.width - (vs.width - 220) / 2 - 20 : (vs.width - 220) / 2 + 20, 60));
    bar->addChild(input_);
    auto sendBtn = NxButton::create(tx("ارسال", "Send"), Size(160, 84), [this] { this->send(); });
    sendBtn->setPosition(fa() ? 100 : vs.width - 100, 60);
    bar->addChild(sendBtn);
    list_ = VList::create(bodySize(124), 10);
    list_->setPosition(Vec2(o.x, o.y + 124));
    addChild(list_);
    showLoading(list_, 4);
    return true;
}

void ChatScene::onEnter() {
    SocialSceneBase::onEnter();
    if (chatId_.empty()) open();
    else reload();
    schedule(CC_SCHEDULE_SELECTOR(ChatScene::poll), 8.f);  // safety net if the realtime socket drops
}

void ChatScene::onExit() {
    unschedule(CC_SCHEDULE_SELECTOR(ChatScene::poll));
    if (channel_) {
        if (auto rt = NxApp::get().online().realtime()) rt->removeChannel(channel_);
        channel_.reset();
    }
    SocialSceneBase::onExit();
}

void ChatScene::open() {
    auto alive = this->alive();
    svc().openDm(peerId_, [this, alive](const SbResult& r, const std::string& chat, const UserCard& peer, const std::string& seen) {
        if (!*alive) return;
        if (!r.ok) { showError(list_, SocialService::friendlyError(r, fa()), [this] { showLoading(list_, 4); open(); }); return; }
        chatId_ = chat;
        peer_ = peer;
        peerLastRead_ = seen;
        // header: avatar + name + @username (tap -> profile)
        Size vs = visibleSize();
        auto who = rowNode(vs.width - 240, 92);
        who->setPosition(vs.width / 2, 54);
        header_->addChild(who, 3);
        float ww = vs.width - 240;
        auto av = Avatar::create(peer_.avatarUrl, peer_.name(), 66);
        av->setPosition(mx(40, ww), 46);
        who->addChild(av);
        textAt(who, ellipsize(peer_.name(), 18), 26, color::text, Weight::Bold, mx(86, ww), 60, true);
        textAt(who, "@" + ellipsize(peer_.username, 18), 18, color::muted, Weight::Regular, mx(86, ww), 28, true);
        std::string id = peer_.id;
        onTap(who, [id] { NxApp::push(ProfileScene::create(id)); });
        subscribe();
        reload();
    });
}

void ChatScene::reload() {
    if (chatId_.empty()) return;
    auto alive = this->alive();
    svc().dmMessages(chatId_, [this, alive](const SbResult& r, const std::vector<DmMessage>& v, const std::string& seen) {
        if (!*alive) return;
        if (!r.ok) {
            if (!loaded_) showError(list_, SocialService::friendlyError(r, fa()), [this] { reload(); });
            return;
        }
        // keep my unsent / failed local messages at the end
        std::vector<DmMessage> local;
        for (auto& m : msgs_)
            if (m.state == DmMessage::State::Sending || m.state == DmMessage::State::Failed) local.push_back(m);
        bool changed = !loaded_ || v.size() != msgs_.size() - local.size() || seen != peerLastRead_;
        if (!changed && !v.empty() && !msgs_.empty()) changed = v.back().id != msgs_[msgs_.size() - local.size() - 1].id;
        msgs_ = v;
        msgs_.insert(msgs_.end(), local.begin(), local.end());
        peerLastRead_ = seen;
        bool hadUnreadFromPeer = !v.empty() && v.back().author != svc().myId();
        loaded_ = true;
        if (changed) render();
        if (hadUnreadFromPeer) svc().markChatRead(chatId_);
    });
}

void ChatScene::poll(float) { reload(); }

void ChatScene::subscribe() {
    auto rt = NxApp::get().online().realtime();
    if (!rt || chatId_.empty()) return;
    channel_ = rt->channel("dm:" + chatId_);
    auto alive = this->alive();
    PostgresChangeSpec ins;
    ins.event = "INSERT";
    ins.table = "nx_messages";
    ins.filter = "chat_id=eq." + chatId_;
    channel_->onPostgresChanges(ins, [this, alive](const Json& rec, const Json&, const std::string&) {
        if (!*alive) return;
        DmMessage m = DmMessage::fromJson(rec);
        for (auto& x : msgs_) if (x.id == m.id) return;
        if (m.author == svc().myId()) return;  // my own arrive through the send callback
        // insert before local pending ones
        auto it = std::find_if(msgs_.begin(), msgs_.end(), [](const DmMessage& x) {
            return x.state == DmMessage::State::Sending || x.state == DmMessage::State::Failed;
        });
        msgs_.insert(it, m);
        render();
        svc().markChatRead(chatId_);
    });
    PostgresChangeSpec rd;
    rd.event = "UPDATE";
    rd.table = "nx_chat_members";
    rd.filter = "chat_id=eq." + chatId_;
    channel_->onPostgresChanges(rd, [this, alive](const Json& rec, const Json&, const std::string&) {
        if (!*alive) return;
        if (rec["user_id"].isString() && rec["user_id"].asString() == peer_.id && rec["last_read_at"].isString()) {
            peerLastRead_ = rec["last_read_at"].asString();
            SocialService::applySeen(msgs_, svc().myId(), peerLastRead_);
            render();
        }
    });
    channel_->subscribe();
}

void ChatScene::render(bool keepBottom) {
    list_->clearRows();
    float w = list_->getContentSize().width - 40;
    std::string me = svc().myId();
    if (msgs_.empty()) {
        showEmpty(list_, tx("گفتگو را شروع کن", "Say hello"), tx("پیام‌ها برای همیشه ذخیره می‌شوند.", "Messages are saved to your account."));
        return;
    }
    std::string lastDay;
    for (size_t i = 0; i < msgs_.size(); ++i) {
        const DmMessage& m = msgs_[i];
        int64_t t = SocialService::parseIsoUtc(m.createdAt);
        std::string day = t > 0 ? SocialService::formatLocal(t, "date", fa(), nowSec()) : "";
        if (!day.empty() && day != lastDay) {
            lastDay = day;
            auto d = rowNode(w, 50);
            auto dl = makeText(day, 18, color::muted, Weight::Medium);
            dl->setPosition(w / 2, 25);
            d->addChild(dl);
            list_->addRow(d);
        }
        bool mine = m.author == me;
        std::string body = m.deleted ? tx("این پیام حذف شد", "This message was deleted") : m.text;
        auto label = makeText(body, 25, mine ? color::bg : color::text, Weight::Regular, w * 0.68f,
                              fa() ? TextHAlignment::RIGHT : TextHAlignment::LEFT);
        Size ls = label->getContentSize();
        float bw = std::max(ls.width + 40, 170.f), bh = ls.height + 64;
        auto row = rowNode(w, bh);
        bool right = mine != fa();  // mine on the trailing side
        float bx = right ? w - bw : 0;
        auto bubble = makePanel(Size(bw, bh), mine ? color::f4(color::accent, 0.92f) : color::f4(color::bgMid, 0.95f),
                                color::f4(mine ? color::accent : color::accent2, 0.6f), 24, 1);
        bubble->setPosition(bx, 0);
        row->addChild(bubble);
        label->setPosition(bx + bw / 2, bh - 20 - ls.height / 2);
        row->addChild(label);
        std::string meta = t > 0 ? SocialService::formatLocal(t, "time", fa(), nowSec()) : "";
        if (mine) {
            switch (m.state) {
                case DmMessage::State::Sending: meta += tx("  · در حال ارسال", "  · sending"); break;
                case DmMessage::State::Failed: meta = tx("ارسال نشد · تلاش دوباره", "Not sent · tap to retry"); break;
                case DmMessage::State::Seen: meta += tx("  · دیده شد", "  · seen"); break;
                default: meta += tx("  · ارسال شد", "  · sent"); break;
            }
        }
        auto ml = makeText(meta, 16, mine ? (m.state == DmMessage::State::Failed ? color::danger : color::bgMid) : color::muted);
        ml->setPosition(bx + bw / 2, 20);
        row->addChild(ml);
        if (mine && m.state == DmMessage::State::Failed) {
            std::string lid = m.localId, text = m.text;
            onTap(row, [this, lid, text] {
                msgs_.erase(std::remove_if(msgs_.begin(), msgs_.end(), [&](const DmMessage& x) { return x.localId == lid; }), msgs_.end());
                input_->setText(text.c_str());
                send();
            }, list_);
        }
        list_->addRow(row);
    }
    if (keepBottom) list_->jumpToBottom();
}

void ChatScene::send() {
    std::string text = trim(input_->getText());
    if (text.empty() || chatId_.empty()) return;
    input_->setText("");
    DmMessage local;
    local.localId = "local-" + std::to_string(++localSeq_);
    local.author = svc().myId();
    local.text = text;
    local.state = DmMessage::State::Sending;
    msgs_.push_back(local);
    render();
    auto alive = this->alive();
    std::string lid = local.localId;
    svc().sendDm(chatId_, text, [this, alive, lid](const SbResult& r, const DmMessage& m) {
        if (!*alive) return;
        for (auto& x : msgs_) {
            if (x.localId != lid) continue;
            if (r.ok) {
                std::string keep = x.localId;
                x = m;
                x.localId = keep;
                x.state = DmMessage::State::Sent;
            } else {
                x.state = DmMessage::State::Failed;
                toast(SocialService::friendlyError(r, fa()), true);
            }
        }
        // move the confirmed message before any still-pending ones
        std::stable_partition(msgs_.begin(), msgs_.end(), [](const DmMessage& x) {
            return x.state != DmMessage::State::Sending && x.state != DmMessage::State::Failed;
        });
        SocialService::applySeen(msgs_, svc().myId(), peerLastRead_);
        render();
    });
}

// =====================================================================================================================
// Post detail

PostScene* PostScene::create(const SocialPost& post, const UserCard& author) {
    auto s = new (std::nothrow) PostScene();
    if (s && s->initWith(post, author)) {
        s->autorelease();
        return s;
    }
    delete s;
    return nullptr;
}

bool PostScene::initWith(const SocialPost& post, const UserCard& author) {
    if (!Scene::init()) return false;
    buildBackground();
    addChild(makeTopBar(tx("پست", "Post")), 5);
    auto list = VList::create(bodySize(), 16);
    list->setPosition(visibleOrigin());
    addChild(list);
    float w = list->getContentSize().width - 40;
    auto head = rowNode(w, 100);
    auto av = Avatar::create(author.avatarUrl, author.name(), 76);
    av->setPosition(mx(44, w), 50);
    head->addChild(av);
    textAt(head, author.name(), 26, color::text, Weight::Bold, mx(96, w), 64, true);
    textAt(head, when(post.createdAt, "datetime"), 18, color::muted, Weight::Regular, mx(96, w), 30, true);
    std::string id = author.id;
    onTap(head, [id] { NxApp::push(ProfileScene::create(id)); }, list);
    list->addRow(head);
    if (!post.imageUrl.empty()) list->addRow(ImageBox::create(post.imageUrl, Size(w, w), 20));
    if (!post.text.empty()) {
        auto t = makeText(post.text, 26, color::text, Weight::Regular, w - 20, fa() ? TextHAlignment::RIGHT : TextHAlignment::LEFT);
        auto r = rowNode(w, t->getContentSize().height + 20);
        t->setPosition(w / 2, r->getContentSize().height / 2);
        r->addChild(t);
        list->addRow(r);
    }
    auto stats = rowNode(w, 60);
    textAt(stats, tx("پسند: ", "Likes: ") + num(post.likes) + tx("    نظر: ", "    Comments: ") + num(post.comments), 22, color::muted, Weight::Medium, mx(10, w), 30, true);
    list->addRow(stats);
    return true;
}

// =====================================================================================================================
// Change password

ChangePasswordScene* ChangePasswordScene::create(bool recovery) {
    auto s = new (std::nothrow) ChangePasswordScene();
    if (s && s->initWith(recovery)) {
        s->autorelease();
        return s;
    }
    delete s;
    return nullptr;
}

bool ChangePasswordScene::initWith(bool recovery) {
    if (!Scene::init()) return false;
    recovery_ = recovery;
    buildBackground();
    addChild(makeTopBar(recovery ? tx("رمز عبور جدید", "New password") : tx("تغییر رمز عبور", "Change password")), 5);
    Size vs = visibleSize();
    Vec2 o = visibleOrigin();
    auto root = Node::create();
    root->setPosition(o);
    addChild(root);
    float y = vs.height - 220, w = vs.width - 100;
    auto field = [&](const std::string& ph) {
        auto e = makeInput(Size(w, 84), ph, true);
        e->setPosition(Vec2(vs.width / 2, y));
        root->addChild(e);
        y -= 110;
        return e;
    };
    if (!recovery) current_ = field(tx("رمز فعلی", "Current password"));
    pw_ = field(tx("رمز جدید (حداقل ۸ کاراکتر)", "New password (min 8 characters)"));
    pw2_ = field(tx("تکرار رمز جدید", "Repeat new password"));
    auto go = NxButton::create(tx("ذخیره‌ی رمز", "Save password"), Size(w, 86), [this] { submit(); });
    go->setPosition(vs.width / 2, y);
    root->addChild(go);
    y -= 90;
    error_ = makeText(" ", 22, color::danger, Weight::Bold, w);
    error_->setPosition(vs.width / 2, y);
    root->addChild(error_);
    return true;
}

void ChangePasswordScene::submit() {
    if (busy_) return;
    std::string a = pw_->getText(), b2 = pw2_->getText();
    auto err = [this](const std::string& e) { setText(error_, e.empty() ? " " : e, visibleSize().width - 100); };
    if (a.size() < 8) { err(tx("رمز باید حداقل ۸ کاراکتر باشد.", "Password must be at least 8 characters.")); return; }
    if (a != b2) { err(tx("دو رمز یکسان نیستند.", "The passwords don't match.")); return; }
    auto& on = NxApp::get().online();
    if (!on.isOnline()) { err(tx("ابتدا وارد حساب شو.", "Sign in first.")); return; }
    busy_ = true;
    err("");
    auto alive = this->alive();
    auto change = [this, alive, a, err]() {
        NxApp::get().online().changePassword(a, [this, alive, err](bool ok) {
            if (!*alive) return;
            busy_ = false;
            if (!ok) {
                SbResult r;
                r.error = NxApp::get().online().lastError();
                r.status = 400;
                err(SocialService::friendlyError(r, fa()));
                return;
            }
            toast(tx("رمز عبور تغییر کرد.", "Password changed."));
            NxApp::pop();
        });
    };
    if (recovery_) { change(); return; }
    // Re-check the current password before changing it (protects an unlocked phone).
    std::string id = on.isInternalAccount() ? on.email().substr(0, on.email().find('@')) : on.email();
    on.signInEmail(id, current_->getText(), [this, alive, change, err](bool ok) {
        if (!*alive) return;
        if (!ok) {
            busy_ = false;
            err(tx("رمز فعلی نادرست است.", "Current password is wrong."));
            return;
        }
        change();
    });
}

// =====================================================================================================================

bool SignInRequiredScene::init() {
    if (!Scene::init()) return false;
    buildBackground();
    addChild(makeTopBar(tx("ورود لازم است", "Sign in required")), 5);
    Size vs = visibleSize();
    Vec2 o = visibleOrigin();
    auto t = makeText(tx("پروفایل، دنبال کردن و پیام فقط با حساب آنلاین کار می‌کند.",
                         "Profiles, following and messages need an online account."),
                      28, color::text, Weight::Medium, vs.width - 120);
    t->setPosition(o.x + vs.width / 2, o.y + vs.height / 2 + 80);
    addChild(t);
    auto b = NxButton::create(tx("ورود یا ثبت‌نام", "Sign in or create account"), Size(vs.width - 120, 88), [] {
        NxApp::get().settings().setAuthMode("");
        NxApp::replace(AuthScene::create());
    });
    b->setPosition(o.x + vs.width / 2, o.y + vs.height / 2 - 60);
    addChild(b);
    return true;
}

}  // namespace nx
