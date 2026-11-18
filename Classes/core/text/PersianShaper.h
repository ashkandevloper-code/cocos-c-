// Minimal Arabic/Persian shaping + bidi for cocos2d-x v4 (its FreeType labels do no shaping).
//  * shape(): picks contextual presentation forms (U+FB50..U+FEFF), lam-alef ligatures, honours ZWNJ/ZWJ
//    and transparent harakat.
//  * visual(): reorders a logical string to visual (left-to-right glyph) order: RTL runs are reversed,
//    LTR runs (Latin words, numbers) keep their order, brackets are mirrored inside RTL runs.
// Use toDisplay() on any UI string before handing it to a Label.
#pragma once
#include <functional>
#include <string>

namespace nx {
namespace text {

std::u32string utf8To32(const std::string& s);
std::string utf32To8(const std::u32string& s);

bool isRtlChar(char32_t c);
bool containsRtl(const std::string& utf8);

/// Contextual shaping only (logical order kept). ZWNJ/ZWJ are removed from the output.
/// [hasGlyph] lets a font without presentation forms fall back to the base letter.
std::u32string shape(const std::u32string& logical, const std::function<bool(char32_t)>& hasGlyph = nullptr);

/// Bidi reorder of an already shaped line. [rtlParagraph] = base direction.
std::u32string visual(const std::u32string& shaped, bool rtlParagraph);

/// shape + visual, line by line ('\n' kept). Strings with no RTL letters are returned unchanged.
std::string toDisplay(const std::string& utf8, const std::function<bool(char32_t)>& hasGlyph = nullptr);

/// Persian digits <-> ASCII (OTP fields, scores).
std::string toPersianDigits(const std::string& utf8);
std::string normalizeDigits(const std::string& utf8);

}  // namespace text
}  // namespace nx
