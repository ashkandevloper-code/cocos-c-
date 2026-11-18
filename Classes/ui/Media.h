// Network images for the social screens: disk-cached downloads, round avatars with an initials placeholder,
// cover-cropped image boxes and skeleton placeholders. Every widget has a fixed content size from the start,
// so nothing jumps when the picture arrives.
#pragma once
#include "cocos2d.h"

#include <functional>
#include <memory>
#include <string>

namespace nx {
namespace ui {

/// Downloads [url] once into the writable dir and reports the local file ("" on failure). Callbacks run on the
/// cocos thread; requests for the same URL are coalesced.
void fetchImage(const std::string& url, std::function<void(const std::string& localPath)> cb);

class Avatar : public cocos2d::Node {
public:
    static Avatar* create(const std::string& url, const std::string& name, float diameter);
    void setUrl(const std::string& url, const std::string& name);
    void setLocalFile(const std::string& path);  // preview right after picking
    ~Avatar() override;

private:
    bool initWith(float d);
    void showPlaceholder(const std::string& name);
    void showTexture(cocos2d::Texture2D* tex);
    float d_ = 64;
    std::string url_;
    cocos2d::ClippingNode* clip_ = nullptr;
    cocos2d::Node* placeholder_ = nullptr;
    std::shared_ptr<bool> alive_ = std::make_shared<bool>(true);
};

/// Image cover-cropped into [size] with rounded clipping; shows a skeleton until loaded.
class ImageBox : public cocos2d::Node {
public:
    static ImageBox* create(const std::string& url, const cocos2d::Size& size, float radius = 16.f);
    ~ImageBox() override;

private:
    bool initWith(const std::string& url, const cocos2d::Size& size, float radius);
    cocos2d::ClippingNode* clip_ = nullptr;
    std::shared_ptr<bool> alive_ = std::make_shared<bool>(true);
};

/// Pulsing grey rounded rectangle used while data loads.
cocos2d::Node* makeSkeleton(const cocos2d::Size& size, float radius = 14.f);

/// Calls [fn] on a tap (not a drag) inside [n]; [clip] (e.g. the scroll view) must also contain the touch.
void onTap(cocos2d::Node* n, std::function<void()> fn, cocos2d::Node* clip = nullptr);

}  // namespace ui
}  // namespace nx
