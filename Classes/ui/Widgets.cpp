#include "ui/Widgets.h"

#include "core/AppSettings.h"
#include "core/text/PersianShaper.h"

#include <cmath>

USING_NS_CC;

namespace nx {
namespace ui {

Size visibleSize() { return Director::getInstance()->getVisibleSize(); }
Vec2 visibleOrigin() { return Director::getInstance()->getVisibleOrigin(); }

std::string fontFor(const std::string& s, Weight w) {
    bool rtl = text::containsRtl(s);
    if (rtl) {
        switch (w) {
            case Weight::Regular: return "fonts/Estedad-Regular.ttf";
            case Weight::Medium: return "fonts/Estedad-Medium.ttf";
            case Weight::Bold: return "fonts/Estedad-Bold.ttf";
            case Weight::Black: return "fonts/Estedad-Black.ttf";
            case Weight::Display: return "fonts/Estedad-Black.ttf";  // Lalezar lacks isolated alef forms
        }
    }
    switch (w) {
        case Weight::Regular: return "fonts/ChakraPetch-Regular.ttf";
        case Weight::Medium: return "fonts/ChakraPetch-SemiBold.ttf";
        default: return "fonts/ChakraPetch-Bold.ttf";
    }
}

namespace {

float measure(const std::string& display, const std::string& font, float size) {
    TTFConfig cfg(font, size);
    auto l = Label::createWithTTF(cfg, display);
    return l ? l->getContentSize().width : 0.f;
}

/// Logical-order greedy wrap for RTL strings, each line shaped separately (cocos would otherwise
/// wrap the reversed visual string and put lines in the wrong order).
std::string wrapRtl(const std::string& logical, const std::string& font, float size, float maxWidth) {
    std::string out;
    std::string line;
    size_t i = 0;
    auto flush = [&]() {
        if (!out.empty()) out += "\n";
        out += text::toDisplay(line);
        line.clear();
    };
    while (i <= logical.size()) {
        size_t sp = logical.find_first_of(" \n", i);
        if (sp == std::string::npos) sp = logical.size();
        std::string word = logical.substr(i, sp - i);
        std::string cand = line.empty() ? word : line + " " + word;
        if (!line.empty() && measure(text::toDisplay(cand), font, size) > maxWidth) {
            flush();
            line = word;
        } else {
            line = cand;
        }
        if (sp < logical.size() && logical[sp] == '\n') flush();
        i = sp + 1;
    }
    if (!line.empty()) flush();
    return out;
}

}  // namespace

static std::string displayFor(const std::string& s, const std::string& font, float size, float maxWidth) {
    if (!text::containsRtl(s)) return s;
    if (maxWidth > 0) return wrapRtl(s, font, size, maxWidth);
    return text::toDisplay(s);
}

Label* makeText(const std::string& s, float size, const Color3B& color, Weight w, float maxWidth, TextHAlignment align) {
    std::string font = fontFor(s, w);
    TTFConfig cfg(font, size);
    bool rtl = text::containsRtl(s);
    auto l = Label::createWithTTF(cfg, displayFor(s, font, size, maxWidth));
    if (!l) l = Label::createWithSystemFont(s, "sans-serif", size);
    l->setTextColor(Color4B(color));
    if (maxWidth > 0) {
        if (!rtl) l->setMaxLineWidth(maxWidth);
        l->setAlignment(rtl && align == TextHAlignment::LEFT ? TextHAlignment::RIGHT : align);
    }
    l->setName(font);
    l->setUserObject(nullptr);
    return l;
}

void setText(Label* l, const std::string& s, float maxWidth) {
    if (!l) return;
    std::string font = fontFor(s, Weight::Regular);
    // Keep the label's own font family/weight when the script does not change.
    const std::string& cur = l->getTTFConfig().fontFilePath;
    bool curRtl = cur.find("Estedad") != std::string::npos;
    bool rtl = text::containsRtl(s);
    if (curRtl != rtl) {
        TTFConfig cfg = l->getTTFConfig();
        cfg.fontFilePath = font;
        l->setTTFConfig(cfg);
    }
    l->setString(displayFor(s, l->getTTFConfig().fontFilePath, l->getTTFConfig().fontSize, maxWidth));
}

void drawRoundRect(DrawNode* d, const Rect& r, float radius, const Color4F& fill, const Color4F& border, float borderWidth) {
    radius = std::min(radius, std::min(r.size.width, r.size.height) / 2.f);
    std::vector<Vec2> pts;
    const int seg = 8;
    struct C { float cx, cy, a0; } corners[4] = {
        {r.getMaxX() - radius, r.getMaxY() - radius, 0.f},
        {r.getMinX() + radius, r.getMaxY() - radius, 90.f},
        {r.getMinX() + radius, r.getMinY() + radius, 180.f},
        {r.getMaxX() - radius, r.getMinY() + radius, 270.f},
    };
    for (auto& c : corners) {
        for (int i = 0; i <= seg; ++i) {
            float a = CC_DEGREES_TO_RADIANS(c.a0 + 90.f * i / seg);
            pts.emplace_back(c.cx + radius * std::cos(a), c.cy + radius * std::sin(a));
        }
    }
    d->drawPolygon(pts.data(), static_cast<int>(pts.size()), fill, borderWidth, border);
}

DrawNode* makePanel(const Size& size, const Color4F& fill, const Color4F& border, float radius, float borderWidth) {
    auto d = DrawNode::create();
    d->setContentSize(size);
    drawRoundRect(d, Rect(0, 0, size.width, size.height), radius, fill, border, borderWidth);
    return d;
}

// ---------------- NxButton ----------------

NxButton* NxButton::create(const std::string& title, const Size& size, std::function<void()> onTap, ButtonStyle style) {
    auto b = new (std::nothrow) NxButton();
    if (b && b->init(title, size, std::move(onTap), style)) {
        b->autorelease();
        return b;
    }
    delete b;
    return nullptr;
}

bool NxButton::init(const std::string& title, const Size& size, std::function<void()> onTap, ButtonStyle style) {
    if (!Node::init()) return false;
    setContentSize(size);
    setAnchorPoint(Vec2(0.5f, 0.5f));
    style_ = style;
    onTap_ = std::move(onTap);
    bg_ = DrawNode::create();
    addChild(bg_);
    label_ = makeText(title, std::min(30.f, size.height * 0.36f), style == ButtonStyle::Primary ? color::bg : color::text,
                      Weight::Bold);
    label_->setPosition(size.width / 2, size.height / 2);
    addChild(label_, 1);
    redraw(false);

    auto touch = EventListenerTouchOneByOne::create();
    touch->setSwallowTouches(true);
    touch->onTouchBegan = [this](Touch* t, Event*) {
        if (!enabled_ || !isVisible()) return false;
        for (Node* p = getParent(); p; p = p->getParent()) if (!p->isVisible()) return false;
        Vec2 local = convertToNodeSpace(t->getLocation());
        Rect r(0, 0, getContentSize().width, getContentSize().height);
        if (!r.containsPoint(local)) return false;
        redraw(true);
        return true;
    };
    touch->onTouchMoved = [this](Touch* t, Event*) {
        if ((t->getLocation() - t->getStartLocation()).length() > 24.f) redraw(false);
    };
    touch->onTouchEnded = [this](Touch* t, Event*) {
        redraw(false);
        Vec2 local = convertToNodeSpace(t->getLocation());
        Rect r(0, 0, getContentSize().width, getContentSize().height);
        if (r.containsPoint(local) && (t->getLocation() - t->getStartLocation()).length() <= 24.f && onTap_) {
            auto cb = onTap_;  // the callback may replace the scene
            cb();
        }
    };
    touch->onTouchCancelled = [this](Touch*, Event*) { redraw(false); };
    _eventDispatcher->addEventListenerWithSceneGraphPriority(touch, this);
    return true;
}

void NxButton::redraw(bool pressed) {
    bg_->clear();
    Size s = getContentSize();
    Color4F fill, border;
    switch (style_) {
        case ButtonStyle::Primary: fill = color::f4(color::accent); border = color::f4(color::text, 0.6f); break;
        case ButtonStyle::Secondary: fill = color::f4(color::accent2, 0.85f); border = color::f4(color::accent2); break;
        case ButtonStyle::Ghost: fill = Color4F(1, 1, 1, 0.06f); border = color::f4(color::accent, 0.7f); break;
        case ButtonStyle::Danger: fill = color::f4(color::danger, 0.9f); border = color::f4(color::danger); break;
    }
    if (pressed) { fill.r *= 0.8f; fill.g *= 0.8f; fill.b *= 0.8f; }
    if (!enabled_) fill.a *= 0.4f;
    drawRoundRect(bg_, Rect(0, 0, s.width, s.height), s.height / 2.f, fill, border, 1.5f);
    setScale(pressed ? 0.97f : 1.f);
}

void NxButton::setTitle(const std::string& t) { setText(label_, t); }
void NxButton::setEnabled(bool e) {
    enabled_ = e;
    redraw(false);
    label_->setOpacity(e ? 255 : 120);
}

// ---------------- inputs / images / chrome ----------------

cocos2d::ui::EditBox* makeInput(const Size& size, const std::string& placeholder, bool password,
                                cocos2d::ui::EditBox::InputMode mode) {
    auto bg = cocos2d::ui::Scale9Sprite::create("ui/cocos/white.png");
    bg->setColor(color::bgMid);
    bg->setOpacity(220);
    auto e = cocos2d::ui::EditBox::create(size, bg);
    e->setFontName("fonts/Estedad-Regular.ttf");
    e->setFontSize(static_cast<int>(size.height * 0.38f));
    e->setFontColor(color::text);
    e->setPlaceHolder(placeholder.c_str());  // native placeholder: OS shapes Persian itself
    e->setPlaceholderFontName("fonts/Estedad-Regular.ttf");
    e->setPlaceholderFontSize(static_cast<int>(size.height * 0.34f));
    e->setPlaceholderFontColor(color::muted);
    e->setInputMode(mode);
    if (password) e->setInputFlag(cocos2d::ui::EditBox::InputFlag::PASSWORD);
    e->setReturnType(cocos2d::ui::EditBox::KeyboardReturnType::DONE);
    return e;
}

Node* makeSpaceBackground(const Size& size, bool animated) {
    auto root = Node::create();
    root->setContentSize(size);
    auto grad = LayerGradient::create(Color4B(color::bg), Color4B(color::bgMid));
    grad->setContentSize(size);
    root->addChild(grad);
    auto stars = DrawNode::create();
    unsigned seed = 1234567u;
    auto rnd = [&seed]() { seed = seed * 1103515245u + 12345u; return ((seed >> 8) & 0xffff) / 65535.f; };
    for (int i = 0; i < 140; ++i) {
        float r = 0.6f + rnd() * 1.8f;
        stars->drawDot(Vec2(rnd() * size.width, rnd() * size.height), r, Color4F(0.9f, 0.9f, 1.f, 0.25f + rnd() * 0.6f));
    }
    root->addChild(stars);
    if (animated) {
        stars->runAction(RepeatForever::create(Sequence::create(FadeTo::create(2.4f, 150), FadeTo::create(2.4f, 255), nullptr)));
    }
    return root;
}

Node* makeImage(const std::string& path, const Size& box) {
    if (!FileUtils::getInstance()->isFileExist(path)) {
        auto n = Node::create();
        n->setContentSize(box);
        n->setAnchorPoint(Vec2(0.5f, 0.5f));
        return n;
    }
    auto s = Sprite::create(path);
    if (!s) {
        auto n = Node::create();
        n->setContentSize(box);
        n->setAnchorPoint(Vec2(0.5f, 0.5f));
        return n;
    }
    Size c = s->getContentSize();
    if (c.width > 0 && c.height > 0) s->setScale(std::min(box.width / c.width, box.height / c.height));
    return s;
}

Node* makeTopBar(const std::string& title, std::function<void()> onBack) {
    Size vs = visibleSize();
    Vec2 o = visibleOrigin();
    auto bar = Node::create();
    bar->setContentSize(Size(vs.width, 110));
    bar->setPosition(o.x, o.y + vs.height - 110);
    auto bg = makePanel(Size(vs.width - 24, 92), color::f4(color::bgMid, 0.85f), color::f4(color::accent2, 0.6f), 26);
    bg->setPosition(12, 8);
    bar->addChild(bg);
    auto t = makeText(title, 34, color::text, Weight::Bold);
    t->setPosition(vs.width / 2, 54);
    bar->addChild(t);
    auto back = NxButton::create(nx::uiIsFa() ? ">" : "<", Size(84, 64), [onBack]() {
        if (onBack) onBack();
        else Director::getInstance()->popScene();
    }, ButtonStyle::Ghost);
    back->setPosition(nx::uiIsFa() ? vs.width - 66 : 66, 54);
    bar->addChild(back, 2);
    return bar;
}

void toast(const std::string& msg, bool error) {
    auto scene = Director::getInstance()->getRunningScene();
    if (!scene) return;
    Size vs = visibleSize();
    Vec2 o = visibleOrigin();
    auto l = makeText(msg, 26, color::text, Weight::Medium, vs.width - 120);
    Size ls = l->getContentSize();
    auto panel = makePanel(Size(ls.width + 60, ls.height + 36), color::f4(error ? color::danger : color::bgMid, 0.95f),
                           color::f4(error ? color::danger : color::accent), 22);
    panel->setPosition(o.x + (vs.width - ls.width - 60) / 2, o.y + 120);
    l->setPosition((ls.width + 60) / 2, (ls.height + 36) / 2);
    panel->addChild(l);
    scene->addChild(panel, 10000);
    panel->runAction(Sequence::create(DelayTime::create(2.6f), FadeOut::create(0.3f), RemoveSelf::create(), nullptr));
}

// ---------------- VList ----------------

VList* VList::create(const Size& size, float gap) {
    auto v = new (std::nothrow) VList();
    if (v && v->init()) {
        v->autorelease();
        v->gap_ = gap;
        v->setDirection(cocos2d::ui::ScrollView::Direction::VERTICAL);
        v->setContentSize(size);
        v->setBounceEnabled(true);
        v->setScrollBarEnabled(false);
        v->setInnerContainerSize(size);
        return v;
    }
    delete v;
    return nullptr;
}

void VList::addRow(Node* n) {
    rows_.push_back(n);
    addChild(n);
    relayout();
}

void VList::clearRows() {
    for (auto n : rows_) n->removeFromParent();
    rows_.clear();
    relayout();
}

void VList::relayout() {
    float total = gap_;
    for (auto n : rows_) total += n->getContentSize().height * n->getScaleY() + gap_;
    Size s = getContentSize();
    float h = std::max(total, s.height);
    setInnerContainerSize(Size(s.width, h));
    float y = h - gap_;
    for (auto n : rows_) {
        float rh = n->getContentSize().height * n->getScaleY();
        Vec2 ap = n->getAnchorPoint();
        n->setPosition(s.width / 2 + (ap.x - 0.5f) * n->getContentSize().width, y - rh * (1.f - ap.y));
        y -= rh + gap_;
    }
    jumpToTop();
}

}  // namespace ui
}  // namespace nx
