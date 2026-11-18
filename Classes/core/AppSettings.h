// Port of lib/core/settings/app_settings.dart (same storage keys and defaults).
#pragma once
#include "backend/Transport.h"

#include <functional>
#include <string>
#include <vector>

namespace nx {

enum class GraphicsQuality { Low, Medium, High, Ultra };

class AppSettings {
public:
    explicit AppSettings(IKeyValueStore* store);

    GraphicsQuality quality() const { return quality_; }
    bool reducedMotion() const { return reducedMotion_; }
    const std::string& languageCode() const { return lang_; }
    bool isFa() const { return lang_ == "fa"; }
    bool music() const { return music_; }
    double musicVolume() const { return musicVolume_; }
    bool sound() const { return sound_; }
    bool haptics() const { return haptics_; }
    bool journeySeen() const { return journeySeen_; }
    bool onboarded() const { return onboarded_; }
    const std::string& authMode() const { return authMode_; }  // "" | "guest" | "account"
    bool oracleBuddy() const { return oracleBuddy_; }
    bool oracleTalk() const { return oracleTalk_; }
    bool fancyEffects() const { return quality_ == GraphicsQuality::High || quality_ == GraphicsQuality::Ultra; }

    void setQuality(GraphicsQuality q);
    void setReducedMotion(bool v);
    void setLanguage(const std::string& code);
    void setMusic(bool v);
    void setMusicVolume(double v);
    void setSound(bool v);
    void setHaptics(bool v);
    void setJourneySeen(bool v);
    void setOnboarded(bool v);
    void setAuthMode(const std::string& v);
    void setOracleBuddy(bool v);
    void setOracleTalk(bool v);

    void addListener(std::function<void()> l) { listeners_.push_back(std::move(l)); }

    static const char* qualityName(GraphicsQuality q);

private:
    void notify();
    IKeyValueStore* store_;
    GraphicsQuality quality_ = GraphicsQuality::High;
    bool reducedMotion_ = false;
    std::string lang_ = "fa";
    bool music_ = true;
    double musicVolume_ = 0.6;
    bool sound_ = true, haptics_ = true, journeySeen_ = false, onboarded_ = false;
    std::string authMode_;
    bool oracleBuddy_ = true, oracleTalk_ = true;
    std::vector<std::function<void()>> listeners_;
};

/// Global language switch used by tx() (mirrors context.tx(fa, en)).
void setUiLanguageFa(bool fa);
bool uiIsFa();
inline std::string tx(const std::string& fa, const std::string& en) { return uiIsFa() ? fa : en; }

}  // namespace nx
