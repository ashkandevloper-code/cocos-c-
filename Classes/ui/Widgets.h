// NEXORA UI kit for cocos2d-x (port of the NX2 neon design tokens in lib/core/theme + nx2_widgets).
// Every text goes through makeText(), which shapes Persian (RTL) text and picks Estedad / ChakraPetch.
#pragma once
#include "cocos2d.h"
#include "ui/CocosGUI.h"

#include <functional>
#include <string>

namespace nx {
namespace ui {

namespace color {
const cocos2d::Color3B accent(0x35, 0xD6, 0xFF);
const cocos2d::Color3B accent2(0xB0, 0x4D, 0xFF);
const cocos2d::Color3B bg(0x07, 0x03, 0x1A);
const cocos2d::Color3B bgMid(0x14, 0x0A, 0x3A);
const cocos2d::Color3B text(0xF4, 0xF0, 0xFF);
const cocos2d::Color3B muted(0xBF, 0xB3, 0xE6);
const cocos2d::Color3B gold(0xFF, 0xC8, 0x57);
const cocos2d::Color3B pink(0xFF, 0x4F, 0xA3);
const cocos2d::Color3B danger(0xFF, 0x5A, 0x6E);
const cocos2d::Color3B success(0x3D, 0xDC, 0x97);
const cocos2d::Color3B turquoise(0x2E, 0xE6, 0xD6);
inline cocos2d::Color4F f4(const cocos2d::Color3B& c, float a = 1.f) {
    return cocos2d::Color4F(c.r / 255.f, c.g / 255.f, c.b / 255.f, a);
}
inline cocos2d::Color3B hex(unsigned rgb) {
    return cocos2d::Color3B((rgb >> 16) & 0xff, (rgb >> 8) & 0xff, rgb & 0xff);
}
}  // namespace color

enum class Weight { Regular, Medium, Bold, Black, Display };

/// Font file for a string + weight (Estedad for Persian, ChakraPetch for Latin, Lalezar for display Persian).
std::string fontFor(const std::string& text, Weight w);

/// Label with RTL shaping/bidi and (for RTL) logical-order word wrapping when maxWidth > 0.
cocos2d::Label* makeText(const std::string& text, float size, const cocos2d::Color3B& color = color::text,
                         Weight w = Weight::Regular, float maxWidth = 0,
                         cocos2d::TextHAlignment align = cocos2d::TextHAlignment::CENTER);
/// Re-sets a label's string through the same shaping path.
void setText(cocos2d::Label* l, const std::string& text, float maxWidth = 0);

/// Rounded rectangle drawn with DrawNode (anchor = bottom-left, content size = size).
cocos2d::DrawNode* makePanel(const cocos2d::Size& size, const cocos2d::Color4F& fill, const cocos2d::Color4F& border,
                             float radius = 18.f, float borderWidth = 1.5f);
void drawRoundRect(cocos2d::DrawNode* d, const cocos2d::Rect& r, float radius, const cocos2d::Color4F& fill,
                   const cocos2d::Color4F& border, float borderWidth);

enum class ButtonStyle { Primary, Secondary, Ghost, Danger };

class NxButton : public cocos2d::Node {
public:
    static NxButton* create(const std::string& title, const cocos2d::Size& size, std::function<void()> onTap,
                            ButtonStyle style = ButtonStyle::Primary);
    void setTitle(const std::string& t);
    void setEnabled(bool e);
    bool isEnabled() const { return enabled_; }

private:
    bool init(const std::string& title, const cocos2d::Size& size, std::function<void()> onTap, ButtonStyle style);
    void redraw(bool pressed);
    cocos2d::DrawNode* bg_ = nullptr;
    cocos2d::Label* label_ = nullptr;
    std::function<void()> onTap_;
    ButtonStyle style_ = ButtonStyle::Primary;
    bool enabled_ = true;
};

/// Single-line input (native EditBox; Persian typing handled by the OS keyboard).
cocos2d::ui::EditBox* makeInput(const cocos2d::Size& size, const std::string& placeholder, bool password = false,
                                cocos2d::ui::EditBox::InputMode mode = cocos2d::ui::EditBox::InputMode::SINGLE_LINE);

/// Deep-space background with twinkling stars (cheap DrawNode points).
cocos2d::Node* makeSpaceBackground(const cocos2d::Size& size, bool animated = true);

/// Sprite scaled to fit [box] (keeps aspect). Returns an empty Node when the file is missing.
cocos2d::Node* makeImage(const std::string& path, const cocos2d::Size& box);

/// Top bar with title + back arrow (back = pop scene or [onBack]).
cocos2d::Node* makeTopBar(const std::string& title, std::function<void()> onBack = nullptr);

/// Short floating message at the bottom of the running scene.
void toast(const std::string& text, bool error = false);

/// Vertical scroll list helper: returns a ScrollView whose inner container grows with addRow().
class VList : public cocos2d::ui::ScrollView {
public:
    static VList* create(const cocos2d::Size& size, float gap = 14.f);
    void addRow(cocos2d::Node* n);  // n: anchor (0.5, 0.5), uses contentSize height
    void clearRows();

private:
    float gap_ = 14.f;
    std::vector<cocos2d::Node*> rows_;
    void relayout();
};

cocos2d::Size visibleSize();
cocos2d::Vec2 visibleOrigin();

}  // namespace ui
}  // namespace nx
