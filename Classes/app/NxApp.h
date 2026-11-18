// Application services (Dart: ProviderScope + Riverpod providers): settings, online service, transports.
#pragma once
#include "backend/OnlineService.h"
#include "core/AppSettings.h"

#include <memory>

namespace cocos2d { class Scene; }

namespace nx {

class CurlHttp;
class UserDefaultStore;

class NxApp {
public:
    static NxApp& get();
    void init();  // after the GLView exists

    AppSettings& settings() { return *settings_; }
    OnlineService& online() { return *online_; }
    IKeyValueStore& store();

    /// Display name used in games / profile (local profile name or the account e-mail prefix).
    std::string playerName();
    void setPlayerName(const std::string& n);

    // Navigation (go_router equivalent)
    static void replace(cocos2d::Scene* s);  // top-level flow (intro -> auth -> hub)
    static void push(cocos2d::Scene* s);     // world / game on top of the hub
    static void pop();

private:
    NxApp() = default;
    std::unique_ptr<UserDefaultStore> store_;
    std::unique_ptr<CurlHttp> http_;
    std::unique_ptr<AppSettings> settings_;
    std::unique_ptr<OnlineService> online_;
    bool ready_ = false;
};

}  // namespace nx
