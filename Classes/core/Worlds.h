// Port of lib/worlds/world_registry.dart: the seven worlds shown on the Hub.
#pragma once
#include <string>
#include <vector>

namespace nx {

struct NexoraWorld {
    std::string id, name, nameFa, taglineFa, taglineEn;
    unsigned color;  // palette accent 0xRRGGBB
    std::vector<std::string> keywords;
    std::string portal() const { return "worlds/hub/portals/" + id + ".webp"; }
};

inline const std::vector<NexoraWorld>& worlds() {
    static const std::vector<NexoraWorld> w = {
        {"nova_gallery", "NOVA GALLERY", "نوا گالری", "پست، استوری و ریلز — دنیای اجتماعی", "Posts, stories & reels — the social world", 0xFF4FA3, {"post", "social", "پست", "گالری", "استوری"}},
        {"orbit_network", "ORBIT NETWORK", "شبکه‌ی مدار", "پیام، گروه، کانال و تماس", "Messages, groups, channels & calls", 0x00C2FF, {"chat", "message", "پیام", "چت"}},
        {"nexora_arcade", "NEXORA ARCADE", "آرکید نکسورا", "۹ بازی، بات هوشمند و بازی دونفره", "9 games, smart bots & local multiplayer", 0x7B3FF2, {"game", "arcade", "بازی", "آرکید"}},
        {"oracle_core", "ORACLE CORE", "هسته‌ی اوراکل", "دستیار هوشمند همراه تو", "Your intelligent companion", 0x2EE6D6, {"ai", "oracle", "اوراکل", "دستیار"}},
        {"personal_orbit", "PERSONAL ORBIT", "مدار شخصی", "آواتار، فضای شخصی و دستاوردها", "Avatar, your space & achievements", 0xFFC857, {"profile", "avatar", "پروفایل", "آواتار"}},
        {"social_station", "SOCIAL STATION", "ایستگاه اجتماعی", "اجتماعات، رویدادها و مسابقات", "Communities, events & tournaments", 0x3DDC97, {"community", "club", "اجتماع", "کلاب"}},
        {"nexora_exchange", "NEXORA EXCHANGE", "بازار نکسورا", "آیتم‌های آواتار، تزئین و پوسته", "Avatar items, decor & skins", 0xF3C56B, {"shop", "store", "فروشگاه", "بازار"}},
    };
    return w;
}

inline const NexoraWorld* worldById(const std::string& id) {
    for (auto& w : worlds()) if (w.id == id) return &w;
    return nullptr;
}

}  // namespace nx
