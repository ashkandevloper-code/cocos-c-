#include "app/NxApp.h"

#include "cocos2d.h"
#include "platform/CocosTransport.h"
#include "platform/NativeBridge.h"
#include "scenes/ShellScenes.h"
#include "scenes/SocialScenes.h"
#include "ui/Widgets.h"

USING_NS_CC;

namespace nx {

NxApp& NxApp::get() {
    static NxApp app;
    return app;
}

IKeyValueStore& NxApp::store() { return *store_; }
IHttp& NxApp::http() { return *http_; }

void NxApp::init() {
    if (ready_) return;
    ready_ = true;
    store_.reset(new UserDefaultStore());
    http_.reset(new CurlHttp(3));
    settings_.reset(new AppSettings(store_.get()));
    online_.reset(new OnlineService(OnlineConfig::fromBuild(), http_.get(), store_.get(),
                                    []() { return std::unique_ptr<IWebSocket>(new CocosWebSocket()); }));
    social_.reset(new SocialService(online_.get()));
    // E-mail links (magic link, password recovery) arrive as nexora://login-callback#...&refresh_token=..&type=..
    native::setDeepLinkHandler([this](const std::string& url) {
        online_->handleAuthDeepLink(url, [this](bool ok, const std::string& type) {
            if (!ok) {
                ui::toast(tx("لینک ایمیل نامعتبر یا منقضی است.", "The e-mail link is invalid or expired."), true);
                return;
            }
            settings_->setAuthMode("email");
            if (type == "recovery") NxApp::push(ChangePasswordScene::create(true));
            else NxApp::replace(HubScene::create());
        });
    });
    // Realtime heartbeats / reconnects run on the global scheduler, independent of scenes.
    Director::getInstance()->getScheduler()->schedule(
        [this](float) { online_->tick(systemNowMs()); }, this, 0.25f, false, "nx.online.tick");
    CCLOG("NEXORA online configured: %s", online_->configured() ? "yes" : "no (offline build)");
}

std::string NxApp::playerName() {
    std::string n = store_->getString("profile.name", "");
    if (!n.empty()) return n;
    std::string e = online_->email();
    if (!e.empty() && e.find("@users.nexora.invalid") == std::string::npos) return e.substr(0, e.find('@'));
    if (!e.empty()) return e.substr(0, e.find('@'));
    return settings_->isFa() ? "کاوشگر" : "Explorer";
}

void NxApp::setPlayerName(const std::string& n) { store_->setString("profile.name", n); }

void NxApp::replace(Scene* s) {
    auto d = Director::getInstance();
    if (!d->getRunningScene()) d->runWithScene(s);
    else d->replaceScene(TransitionFade::create(0.35f, s, Color3B(7, 3, 26)));
}

void NxApp::push(Scene* s) { Director::getInstance()->pushScene(TransitionFade::create(0.25f, s, Color3B(7, 3, 26))); }

void NxApp::pop() { Director::getInstance()->popScene(); }

void NxApp::openSocial(const std::string& tab) {
    auto& on = get().online();
    if (!on.isOnline() || on.isGuest()) {
        push(SignInRequiredScene::create());
        return;
    }
    push(SocialHomeScene::createAt(tab));
}

}  // namespace nx
