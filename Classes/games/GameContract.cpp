#include "games/GameContract.h"

#include "core/text/PersianShaper.h"

#include <cstdio>

namespace nx {

std::string hashOf(const std::string& utf8) {
    // Dart strings are UTF-16: hash every code unit (surrogate pairs for astral chars).
    std::u32string cps = text::utf8To32(utf8);
    std::vector<uint16_t> units;
    units.reserve(cps.size());
    for (char32_t c : cps) {
        if (c >= 0x10000) {
            c -= 0x10000;
            units.push_back(static_cast<uint16_t>(0xD800 + (c >> 10)));
            units.push_back(static_cast<uint16_t>(0xDC00 + (c & 0x3FF)));
        } else {
            units.push_back(static_cast<uint16_t>(c));
        }
    }
    uint32_t h = 0x811c9dc5u;
    for (uint16_t u : units) {
        h ^= (u & 0xff);
        h *= 0x01000193u;
        h ^= (u >> 8) & 0xff;
        h *= 0x01000193u;
    }
    char buf[16];
    std::snprintf(buf, sizeof buf, "%08x", h);
    return buf;
}

}  // namespace nx
