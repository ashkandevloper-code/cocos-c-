#include "core/text/PersianShaper.h"

#include <algorithm>
#include <vector>

namespace nx {
namespace text {

std::u32string utf8To32(const std::string& s) {
    std::u32string out;
    out.reserve(s.size());
    size_t i = 0;
    while (i < s.size()) {
        unsigned char c = static_cast<unsigned char>(s[i]);
        char32_t cp;
        int n;
        if (c < 0x80) { cp = c; n = 1; }
        else if ((c >> 5) == 6) { cp = c & 0x1f; n = 2; }
        else if ((c >> 4) == 14) { cp = c & 0x0f; n = 3; }
        else if ((c >> 3) == 30) { cp = c & 0x07; n = 4; }
        else { out.push_back(0xFFFD); ++i; continue; }
        if (i + n > s.size()) { out.push_back(0xFFFD); break; }
        bool bad = false;
        for (int k = 1; k < n; ++k) {
            unsigned char cc = static_cast<unsigned char>(s[i + k]);
            if ((cc >> 6) != 2) { bad = true; break; }
            cp = (cp << 6) | (cc & 0x3f);
        }
        if (bad) { out.push_back(0xFFFD); ++i; continue; }
        out.push_back(cp);
        i += n;
    }
    return out;
}

std::string utf32To8(const std::u32string& s) {
    std::string o;
    o.reserve(s.size() * 2);
    for (char32_t c : s) {
        if (c < 0x80) o.push_back(static_cast<char>(c));
        else if (c < 0x800) {
            o.push_back(static_cast<char>(0xC0 | (c >> 6)));
            o.push_back(static_cast<char>(0x80 | (c & 0x3f)));
        } else if (c < 0x10000) {
            o.push_back(static_cast<char>(0xE0 | (c >> 12)));
            o.push_back(static_cast<char>(0x80 | ((c >> 6) & 0x3f)));
            o.push_back(static_cast<char>(0x80 | (c & 0x3f)));
        } else {
            o.push_back(static_cast<char>(0xF0 | (c >> 18)));
            o.push_back(static_cast<char>(0x80 | ((c >> 12) & 0x3f)));
            o.push_back(static_cast<char>(0x80 | ((c >> 6) & 0x3f)));
            o.push_back(static_cast<char>(0x80 | (c & 0x3f)));
        }
    }
    return o;
}

namespace {

constexpr char32_t ZWNJ = 0x200C, ZWJ = 0x200D, TATWEEL = 0x0640;

struct Forms {
    char32_t base;
    char32_t iso, fin, ini, med;  // ini/med == 0 -> right-joining only
};

// Isolated, final, initial, medial.
const Forms kForms[] = {
    {0x0621, 0xFE80, 0, 0, 0},
    {0x0622, 0xFE81, 0xFE82, 0, 0},
    {0x0623, 0xFE83, 0xFE84, 0, 0},
    {0x0624, 0xFE85, 0xFE86, 0, 0},
    {0x0625, 0xFE87, 0xFE88, 0, 0},
    {0x0626, 0xFE89, 0xFE8A, 0xFE8B, 0xFE8C},
    {0x0627, 0xFE8D, 0xFE8E, 0, 0},
    {0x0628, 0xFE8F, 0xFE90, 0xFE91, 0xFE92},
    {0x0629, 0xFE93, 0xFE94, 0, 0},
    {0x062A, 0xFE95, 0xFE96, 0xFE97, 0xFE98},
    {0x062B, 0xFE99, 0xFE9A, 0xFE9B, 0xFE9C},
    {0x062C, 0xFE9D, 0xFE9E, 0xFE9F, 0xFEA0},
    {0x062D, 0xFEA1, 0xFEA2, 0xFEA3, 0xFEA4},
    {0x062E, 0xFEA5, 0xFEA6, 0xFEA7, 0xFEA8},
    {0x062F, 0xFEA9, 0xFEAA, 0, 0},
    {0x0630, 0xFEAB, 0xFEAC, 0, 0},
    {0x0631, 0xFEAD, 0xFEAE, 0, 0},
    {0x0632, 0xFEAF, 0xFEB0, 0, 0},
    {0x0633, 0xFEB1, 0xFEB2, 0xFEB3, 0xFEB4},
    {0x0634, 0xFEB5, 0xFEB6, 0xFEB7, 0xFEB8},
    {0x0635, 0xFEB9, 0xFEBA, 0xFEBB, 0xFEBC},
    {0x0636, 0xFEBD, 0xFEBE, 0xFEBF, 0xFEC0},
    {0x0637, 0xFEC1, 0xFEC2, 0xFEC3, 0xFEC4},
    {0x0638, 0xFEC5, 0xFEC6, 0xFEC7, 0xFEC8},
    {0x0639, 0xFEC9, 0xFECA, 0xFECB, 0xFECC},
    {0x063A, 0xFECD, 0xFECE, 0xFECF, 0xFED0},
    {0x0641, 0xFED1, 0xFED2, 0xFED3, 0xFED4},
    {0x0642, 0xFED5, 0xFED6, 0xFED7, 0xFED8},
    {0x0643, 0xFED9, 0xFEDA, 0xFEDB, 0xFEDC},
    {0x0644, 0xFEDD, 0xFEDE, 0xFEDF, 0xFEE0},
    {0x0645, 0xFEE1, 0xFEE2, 0xFEE3, 0xFEE4},
    {0x0646, 0xFEE5, 0xFEE6, 0xFEE7, 0xFEE8},
    {0x0647, 0xFEE9, 0xFEEA, 0xFEEB, 0xFEEC},
    {0x0648, 0xFEED, 0xFEEE, 0, 0},
    {0x0649, 0xFEEF, 0xFEF0, 0, 0},
    {0x064A, 0xFEF1, 0xFEF2, 0xFEF3, 0xFEF4},
    {0x067E, 0xFB56, 0xFB57, 0xFB58, 0xFB59},
    {0x0686, 0xFB7A, 0xFB7B, 0xFB7C, 0xFB7D},
    {0x0698, 0xFB8A, 0xFB8B, 0, 0},
    {0x06A9, 0xFB8E, 0xFB8F, 0xFB90, 0xFB91},
    {0x06AF, 0xFB92, 0xFB93, 0xFB94, 0xFB95},
    {0x06CC, 0xFBFC, 0xFBFD, 0xFBFE, 0xFBFF},
};

const Forms* formsOf(char32_t c) {
    for (const auto& f : kForms) if (f.base == c) return &f;
    return nullptr;
}

bool isTransparent(char32_t c) {
    return (c >= 0x064B && c <= 0x065F) || c == 0x0670 || (c >= 0x06D6 && c <= 0x06ED);
}
bool joinsToNext(char32_t c) {  // can connect to the following letter (dual-joining or causing)
    if (c == TATWEEL || c == ZWJ) return true;
    const Forms* f = formsOf(c);
    return f && f->ini != 0;
}
bool joinsToPrev(char32_t c) {  // can connect to the preceding letter (right side)
    if (c == TATWEEL || c == ZWJ) return true;
    const Forms* f = formsOf(c);
    return f && f->fin != 0;
}

char32_t lamAlef(char32_t alef, bool fin) {
    switch (alef) {
        case 0x0622: return fin ? 0xFEF6 : 0xFEF5;
        case 0x0623: return fin ? 0xFEF8 : 0xFEF7;
        case 0x0625: return fin ? 0xFEFA : 0xFEF9;
        case 0x0627: return fin ? 0xFEFC : 0xFEFB;
        default: return 0;
    }
}

enum class Dir { L, R, N };

Dir dirOf(char32_t c) {
    if (isRtlChar(c)) return Dir::R;
    if ((c >= '0' && c <= '9') || (c >= 0x06F0 && c <= 0x06F9) || (c >= 0x0660 && c <= 0x0669)) return Dir::L;  // numbers stay LTR
    if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= 0x00C0 && c < 0x0590)) return Dir::L;
    return Dir::N;
}

char32_t mirror(char32_t c) {
    switch (c) {
        case '(': return ')'; case ')': return '(';
        case '[': return ']'; case ']': return '[';
        case '{': return '}'; case '}': return '{';
        case '<': return '>'; case '>': return '<';
        case 0x00AB: return 0x00BB; case 0x00BB: return 0x00AB;
        default: return c;
    }
}

}  // namespace

bool isRtlChar(char32_t c) {
    if (c >= 0x06F0 && c <= 0x06F9) return false;  // extended digits behave as numbers
    if (c >= 0x0660 && c <= 0x0669) return false;
    return (c >= 0x0590 && c <= 0x08FF) || (c >= 0xFB1D && c <= 0xFDFF) || (c >= 0xFE70 && c <= 0xFEFF);
}

bool containsRtl(const std::string& utf8) {
    for (char32_t c : utf8To32(utf8)) if (isRtlChar(c)) return true;
    return false;
}

std::u32string shape(const std::u32string& in, const std::function<bool(char32_t)>& hasGlyph) {
    auto ok = [&](char32_t g) { return g != 0 && (!hasGlyph || hasGlyph(g)); };
    std::u32string out;
    out.reserve(in.size());
    const size_t n = in.size();
    auto prevLetter = [&](size_t i) -> char32_t {  // nearest non-transparent before i (0 if none)
        while (i > 0) {
            --i;
            if (!isTransparent(in[i])) return in[i];
        }
        return 0;
    };
    auto nextLetter = [&](size_t i, size_t* at) -> char32_t {
        for (size_t k = i + 1; k < n; ++k) {
            if (!isTransparent(in[k])) { if (at) *at = k; return in[k]; }
        }
        return 0;
    };
    for (size_t i = 0; i < n; ++i) {
        char32_t c = in[i];
        if (c == ZWNJ || c == ZWJ) continue;
        const Forms* f = formsOf(c);
        if (!f) { out.push_back(c); continue; }
        char32_t p = prevLetter(i);
        bool joinPrev = p != 0 && joinsToNext(p) && f->fin != 0;
        size_t nAt = 0;
        char32_t nx = nextLetter(i, &nAt);
        // Lam + Alef ligature.
        if (c == 0x0644 && nx != 0) {
            char32_t lig = lamAlef(nx, joinPrev);
            if (lig != 0 && ok(lig)) {
                out.push_back(lig);
                for (size_t k = i + 1; k < nAt; ++k) out.push_back(in[k]);  // keep harakat between
                i = nAt;
                continue;
            }
        }
        bool joinNext = f->ini != 0 && nx != 0 && joinsToPrev(nx);
        char32_t g;
        if (joinPrev && joinNext) g = f->med;
        else if (joinPrev) g = f->fin;
        else if (joinNext) g = f->ini;
        else g = f->iso;
        out.push_back(ok(g) ? g : c);
    }
    return out;
}

std::u32string visual(const std::u32string& s, bool rtlParagraph) {
    const size_t n = s.size();
    if (n == 0) return s;
    std::vector<Dir> d(n);
    for (size_t i = 0; i < n; ++i) d[i] = dirOf(s[i]);
    // Resolve neutrals: between equal strong directions -> that direction, else paragraph direction.
    // Number separators ('.', ',', ':', '/') between two LTR chars stay LTR (e.g. 3.14, 12:30).
    for (size_t i = 0; i < n; ++i) {
        if (d[i] != Dir::N) continue;
        size_t j = i;
        while (j < n && d[j] == Dir::N) ++j;
        Dir before = i > 0 ? d[i - 1] : (rtlParagraph ? Dir::R : Dir::L);
        Dir after = j < n ? d[j] : (rtlParagraph ? Dir::R : Dir::L);
        Dir r = (before == after) ? before : (rtlParagraph ? Dir::R : Dir::L);
        for (size_t k = i; k < j; ++k) d[k] = r;
        i = j - 1;
    }
    // Split into runs.
    struct Run { size_t a, b; Dir dir; };
    std::vector<Run> runs;
    size_t start = 0;
    for (size_t i = 1; i <= n; ++i) {
        if (i == n || d[i] != d[start]) {
            runs.push_back({start, i, d[start]});
            start = i;
        }
    }
    std::u32string out;
    out.reserve(n);
    auto emit = [&](const Run& r) {
        if (r.dir == Dir::R) {
            for (size_t k = r.b; k > r.a; --k) out.push_back(mirror(s[k - 1]));
        } else {
            out.append(s, r.a, r.b - r.a);
        }
    };
    if (rtlParagraph) {
        for (size_t k = runs.size(); k > 0; --k) emit(runs[k - 1]);
    } else {
        for (auto& r : runs) emit(r);
    }
    return out;
}

std::string toDisplay(const std::string& utf8, const std::function<bool(char32_t)>& hasGlyph) {
    if (!containsRtl(utf8)) return utf8;
    std::u32string all = utf8To32(utf8);
    std::u32string out;
    size_t a = 0;
    while (a <= all.size()) {
        size_t b = all.find(U'\n', a);
        if (b == std::u32string::npos) b = all.size();
        std::u32string line = all.substr(a, b - a);
        bool rtl = false;
        for (char32_t c : line) {
            Dir dd = dirOf(c);
            if (dd == Dir::R) { rtl = true; break; }
            if (dd == Dir::L && !(c >= '0' && c <= '9') && !(c >= 0x06F0 && c <= 0x06F9)) break;
        }
        out += visual(shape(line, hasGlyph), rtl);
        if (b < all.size()) out.push_back(U'\n');
        a = b + 1;
    }
    return utf32To8(out);
}

std::string toPersianDigits(const std::string& utf8) {
    std::u32string s = utf8To32(utf8);
    for (auto& c : s) if (c >= '0' && c <= '9') c = 0x06F0 + (c - '0');
    return utf32To8(s);
}

std::string normalizeDigits(const std::string& utf8) {
    std::u32string s = utf8To32(utf8), o;
    for (char32_t c : s) {
        if (c >= 0x06F0 && c <= 0x06F9) o.push_back('0' + (c - 0x06F0));
        else if (c >= 0x0660 && c <= 0x0669) o.push_back('0' + (c - 0x0660));
        else if (c == ' ' || c == '\t' || c == '\n' || c == '\r') continue;
        else o.push_back(c);
    }
    return utf32To8(o);
}

}  // namespace text
}  // namespace nx
