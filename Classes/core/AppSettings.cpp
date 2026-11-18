#include "core/AppSettings.h"

#include <algorithm>

namespace nx {

namespace {
const char* kQuality = "settings.quality";
const char* kReduced = "settings.reduced_motion";
const char* kLang = "settings.lang";
const char* kSound = "settings.sound";
const char* kHaptics = "settings.haptics";
const char* kJourney = "settings.journey_seen";
const char* kOnboarded = "settings.onboarded";
const char* kAuthMode = "settings.auth_mode";
const char* kMusic = "settings.music";
const char* kMusicVol = "settings.music_volume";
const char* kOracleBuddy = "settings.oracle_buddy";
const char* kOracleTalk = "settings.oracle_talk";
bool gFa = true;
}  // namespace

void setUiLanguageFa(bool fa) { gFa = fa; }
bool uiIsFa() { return gFa; }

const char* AppSettings::qualityName(GraphicsQuality q) {
    switch (q) {
        case GraphicsQuality::Low: return "low";
        case GraphicsQuality::Medium: return "medium";
        case GraphicsQuality::High: return "high";
        default: return "ultra";
    }
}

AppSettings::AppSettings(IKeyValueStore* store) : store_(store) {
    std::string q = store_->getString(kQuality, "high");
    for (auto g : {GraphicsQuality::Low, GraphicsQuality::Medium, GraphicsQuality::High, GraphicsQuality::Ultra}) {
        if (q == qualityName(g)) quality_ = g;
    }
    reducedMotion_ = store_->getBool(kReduced, false);
    lang_ = store_->getString(kLang, "fa");
    if (lang_ != "fa" && lang_ != "en") lang_ = "fa";
    music_ = store_->getBool(kMusic, true);
    musicVolume_ = std::min(100, std::max(0, store_->getInt(kMusicVol, 60))) / 100.0;
    sound_ = store_->getBool(kSound, true);
    haptics_ = store_->getBool(kHaptics, true);
    journeySeen_ = store_->getBool(kJourney, false);
    onboarded_ = store_->getBool(kOnboarded, false);
    authMode_ = store_->getString(kAuthMode, "");
    oracleBuddy_ = store_->getBool(kOracleBuddy, true);
    oracleTalk_ = store_->getBool(kOracleTalk, true);
    setUiLanguageFa(isFa());
}

void AppSettings::notify() {
    auto ls = listeners_;
    for (auto& l : ls) l();
}

void AppSettings::setQuality(GraphicsQuality q) { quality_ = q; store_->setString(kQuality, qualityName(q)); notify(); }
void AppSettings::setReducedMotion(bool v) { reducedMotion_ = v; store_->setBool(kReduced, v); notify(); }
void AppSettings::setLanguage(const std::string& code) {
    lang_ = code == "en" ? "en" : "fa";
    store_->setString(kLang, lang_);
    setUiLanguageFa(isFa());
    notify();
}
void AppSettings::setMusic(bool v) { music_ = v; store_->setBool(kMusic, v); notify(); }
void AppSettings::setMusicVolume(double v) {
    musicVolume_ = std::min(1.0, std::max(0.0, v));
    store_->setInt(kMusicVol, static_cast<int>(musicVolume_ * 100 + 0.5));
    notify();
}
void AppSettings::setSound(bool v) { sound_ = v; store_->setBool(kSound, v); notify(); }
void AppSettings::setHaptics(bool v) { haptics_ = v; store_->setBool(kHaptics, v); notify(); }
void AppSettings::setJourneySeen(bool v) { journeySeen_ = v; store_->setBool(kJourney, v); notify(); }
void AppSettings::setOnboarded(bool v) { onboarded_ = v; store_->setBool(kOnboarded, v); notify(); }
void AppSettings::setAuthMode(const std::string& v) { authMode_ = v; store_->setString(kAuthMode, v); notify(); }
void AppSettings::setOracleBuddy(bool v) { oracleBuddy_ = v; store_->setBool(kOracleBuddy, v); notify(); }
void AppSettings::setOracleTalk(bool v) { oracleTalk_ = v; store_->setBool(kOracleTalk, v); notify(); }

}  // namespace nx
