// App shell scenes: Intro, Auth, Onboarding, Hub, Settings (+ placeholder for worlds not yet ported).
#pragma once
#include "cocos2d.h"
#include "ui/CocosGUI.h"

#include <string>

namespace nx {

class IntroScene : public cocos2d::Scene {
public:
    CREATE_FUNC(IntroScene);
    bool init() override;
    static void next();  // auth gate (Dart: AuthGate / router redirect)
};

class AuthScene : public cocos2d::Scene {
public:
    CREATE_FUNC(AuthScene);
    bool init() override;

private:
    void build();
    void sendCode();
    void verifyCode();
    void passwordSignIn();
    void continueGuest();
    void signUp();
    void forgot();
    void setError(const std::string& e);
    void setInfo(const std::string& i);
    std::string friendly(const std::string& key, int retryAfter);
    void tickCooldown(float);
    void afterSignIn();

    bool sent_ = false;
    bool passwordMode_ = false;
    bool signUp_ = false;   // create an account (email + username + password)
    bool forgot_ = false;   // send a password-reset e-mail
    bool busy_ = false;
    int cooldown_ = 0;
    cocos2d::Node* form_ = nullptr;
    cocos2d::ui::EditBox* email_ = nullptr;
    cocos2d::ui::EditBox* code_ = nullptr;
    cocos2d::ui::EditBox* password_ = nullptr;
    cocos2d::ui::EditBox* username_ = nullptr;
    cocos2d::ui::EditBox* password2_ = nullptr;
    cocos2d::Label* error_ = nullptr;
    cocos2d::Label* info_ = nullptr;
};

class OnboardingScene : public cocos2d::Scene {
public:
    CREATE_FUNC(OnboardingScene);
    bool init() override;
};

class HubScene : public cocos2d::Scene {
public:
    CREATE_FUNC(HubScene);
    bool init() override;
    void onEnter() override;
    void onExit() override;
    static void openWorld(const std::string& id);

private:
    void refreshStatus();
    cocos2d::Label* status_ = nullptr;
    cocos2d::Label* name_ = nullptr;
    int listenerId_ = -1;
};

class SettingsScene : public cocos2d::Scene {
public:
    CREATE_FUNC(SettingsScene);
    bool init() override;

private:
    void rebuild();
    cocos2d::Node* content_ = nullptr;
};

/// Honest placeholder for a world whose screens are not ported yet (see docs/PORTING.md).
class NotPortedScene : public cocos2d::Scene {
public:
    static NotPortedScene* create(const std::string& titleFa, const std::string& titleEn, const std::string& note);
    bool initWith(const std::string& titleFa, const std::string& titleEn, const std::string& note);
};

}  // namespace nx
