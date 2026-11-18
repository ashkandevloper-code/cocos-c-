#include "ui/Media.h"

#include "app/NxApp.h"
#include "ui/Widgets.h"

#include <map>
#include <vector>

USING_NS_CC;

namespace nx {
namespace ui {

namespace {

std::map<std::string, std::vector<std::function<void(const std::string&)>>> gInflight;

std::string cachePathFor(const std::string& url) {
    uint64_t h = 1469598103934665603ULL;  // FNV-1a
    for (unsigned char c : url) { h ^= c; h *= 1099511628211ULL; }
    std::string ext = ".img";
    size_t q = url.find('?');
    std::string base = url.substr(0, q);
    size_t dot = base.find_last_of('.');
    if (dot != std::string::npos && base.size() - dot <= 5) ext = base.substr(dot);
    std::string dir = FileUtils::getInstance()->getWritablePath() + "nx_img/";
    FileUtils::getInstance()->createDirectory(dir);
    char buf[32];
    snprintf(buf, sizeof buf, "%016llx", static_cast<unsigned long long>(h));
    return dir + buf + ext;
}

Texture2D* textureFor(const std::string& path) {
    if (path.empty()) return nullptr;
    return Director::getInstance()->getTextureCache()->addImage(path);  // jpg / png / webp
}

DrawNode* circleStencil(float d) {
    auto st = DrawNode::create();
    st->drawSolidCircle(Vec2(d / 2, d / 2), d / 2, 0, 64, Color4F::WHITE);
    return st;
}

Color3B colorForName(const std::string& n) {
    static const Color3B palette[] = {color::accent, color::accent2, color::pink, color::turquoise, color::gold, color::success};
    unsigned h = 0;
    for (unsigned char c : n) h = h * 31 + c;
    return palette[h % 6];
}

std::string firstGlyph(const std::string& n) {
    if (n.empty()) return "?";
    unsigned char c = static_cast<unsigned char>(n[0]);
    size_t len = c < 0x80 ? 1 : (c >> 5) == 6 ? 2 : (c >> 4) == 14 ? 3 : 4;
    std::string g = n.substr(0, len);
    if (len == 1 && g[0] >= 'a' && g[0] <= 'z') g[0] = static_cast<char>(g[0] - 32);
    return g;
}

}  // namespace

void fetchImage(const std::string& url, std::function<void(const std::string&)> cb) {
    if (url.empty()) { cb(""); return; }
    if (url.rfind("http", 0) != 0) { cb(FileUtils::getInstance()->isFileExist(url) ? url : ""); return; }
    std::string path = cachePathFor(url);
    if (FileUtils::getInstance()->isFileExist(path)) { cb(path); return; }
    auto& waiters = gInflight[url];
    waiters.push_back(std::move(cb));
    if (waiters.size() > 1) return;
    HttpRequest r;
    r.url = url;
    r.tag = "image";
    NxApp::get().http().send(r, [url, path](const HttpResponse& resp) {
        std::string out;
        if (resp.ok() && !resp.body.empty() && resp.body.size() < 8 * 1024 * 1024) {
            Data d;
            d.copy(reinterpret_cast<const unsigned char*>(resp.body.data()), static_cast<ssize_t>(resp.body.size()));
            if (FileUtils::getInstance()->writeDataToFile(d, path)) out = path;
        }
        auto ws = std::move(gInflight[url]);
        gInflight.erase(url);
        for (auto& w : ws) w(out);
    });
}

// ---------------------------------------------------------------- Avatar

Avatar* Avatar::create(const std::string& url, const std::string& name, float diameter) {
    auto a = new (std::nothrow) Avatar();
    if (a && a->initWith(diameter)) {
        a->autorelease();
        a->setUrl(url, name);
        return a;
    }
    delete a;
    return nullptr;
}

Avatar::~Avatar() { *alive_ = false; }

bool Avatar::initWith(float d) {
    if (!Node::init()) return false;
    d_ = d;
    setContentSize(Size(d, d));
    setAnchorPoint(Vec2(0.5f, 0.5f));
    clip_ = ClippingNode::create(circleStencil(d));
    addChild(clip_, 1);
    auto ring = DrawNode::create();
    ring->drawCircle(Vec2(d / 2, d / 2), d / 2, 0, 64, false, color::f4(color::accent, 0.7f));
    addChild(ring, 2);
    return true;
}

void Avatar::showPlaceholder(const std::string& name) {
    clip_->removeAllChildren();
    auto bg = DrawNode::create();
    Color3B c = colorForName(name);
    bg->drawSolidRect(Vec2::ZERO, Vec2(d_, d_), color::f4(c, 0.35f));
    clip_->addChild(bg);
    auto l = makeText(firstGlyph(name), d_ * 0.42f, color::text, Weight::Black);
    l->setPosition(d_ / 2, d_ / 2);
    clip_->addChild(l);
}

void Avatar::showTexture(Texture2D* tex) {
    if (!tex) return;
    clip_->removeAllChildren();
    auto s = Sprite::createWithTexture(tex);
    Size c = s->getContentSize();
    float sc = std::max(d_ / c.width, d_ / c.height);  // cover
    s->setScale(sc);
    s->setPosition(d_ / 2, d_ / 2);
    clip_->addChild(s);
}

void Avatar::setUrl(const std::string& url, const std::string& name) {
    url_ = url;
    showPlaceholder(name);
    if (url.empty()) return;
    auto alive = alive_;
    std::string want = url;
    fetchImage(url, [this, alive, want](const std::string& path) {
        if (!*alive || want != url_ || path.empty()) return;
        showTexture(textureFor(path));
    });
}

void Avatar::setLocalFile(const std::string& path) {
    url_ = path;
    Director::getInstance()->getTextureCache()->removeTextureForKey(path);
    showTexture(textureFor(path));
}

// ---------------------------------------------------------------- ImageBox

ImageBox* ImageBox::create(const std::string& url, const Size& size, float radius) {
    auto b = new (std::nothrow) ImageBox();
    if (b && b->initWith(url, size, radius)) {
        b->autorelease();
        return b;
    }
    delete b;
    return nullptr;
}

ImageBox::~ImageBox() { *alive_ = false; }

bool ImageBox::initWith(const std::string& url, const Size& size, float radius) {
    if (!Node::init()) return false;
    setContentSize(size);
    setAnchorPoint(Vec2(0.5f, 0.5f));
    auto st = DrawNode::create();
    drawRoundRect(st, Rect(0, 0, size.width, size.height), radius, Color4F::WHITE, Color4F::WHITE, 0);
    clip_ = ClippingNode::create(st);
    addChild(clip_);
    auto sk = makeSkeleton(size, radius);
    sk->setPosition(size.width / 2, size.height / 2);
    clip_->addChild(sk);
    auto alive = alive_;
    fetchImage(url, [this, alive, size](const std::string& path) {
        if (!*alive) return;
        Texture2D* tex = textureFor(path);
        if (!tex) return;
        clip_->removeAllChildren();
        auto s = Sprite::createWithTexture(tex);
        Size c = s->getContentSize();
        s->setScale(std::max(size.width / c.width, size.height / c.height));
        s->setPosition(size.width / 2, size.height / 2);
        s->setOpacity(0);
        s->runAction(FadeIn::create(0.2f));
        clip_->addChild(s);
    });
    return true;
}

// ---------------------------------------------------------------- skeleton / tap

Node* makeSkeleton(const Size& size, float radius) {
    auto holder = Node::create();
    holder->setContentSize(size);
    holder->setAnchorPoint(Vec2(0.5f, 0.5f));
    auto d = DrawNode::create();
    drawRoundRect(d, Rect(0, 0, size.width, size.height), radius, Color4F(1, 1, 1, 0.09f), Color4F(1, 1, 1, 0.0f), 0);
    holder->addChild(d);
    d->runAction(RepeatForever::create(Sequence::create(FadeTo::create(0.6f, 110), FadeTo::create(0.6f, 255), nullptr)));
    return holder;
}

void onTap(Node* n, std::function<void()> fn, Node* clip) {
    auto l = EventListenerTouchOneByOne::create();
    l->onTouchBegan = [n, clip](Touch* t, Event*) {
        if (!n->isVisible()) return false;
        for (Node* p = n->getParent(); p; p = p->getParent())
            if (!p->isVisible()) return false;
        Vec2 p = n->convertToNodeSpace(t->getLocation());
        Rect r(0, 0, n->getContentSize().width, n->getContentSize().height);
        if (!r.containsPoint(p)) return false;
        if (clip) {
            Vec2 cp = clip->convertToNodeSpace(t->getLocation());
            if (!Rect(0, 0, clip->getContentSize().width, clip->getContentSize().height).containsPoint(cp)) return false;
        }
        return true;
    };
    l->onTouchEnded = [fn](Touch* t, Event*) {
        if ((t->getLocation() - t->getStartLocation()).length() < 20) fn();
    };
    n->getEventDispatcher()->addEventListenerWithSceneGraphPriority(l, n);
}

}  // namespace ui
}  // namespace nx
