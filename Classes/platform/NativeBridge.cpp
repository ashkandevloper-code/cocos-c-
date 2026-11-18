#include "platform/NativeBridge.h"

#include "cocos2d.h"
#if CC_TARGET_PLATFORM == CC_PLATFORM_ANDROID
#include "platform/android/jni/JniHelper.h"
#endif

namespace nx {
namespace native {

namespace {
#if CC_TARGET_PLATFORM == CC_PLATFORM_ANDROID
const char* kActivity = "com/nexora/cocos/AppActivity";
#endif
std::function<void(const std::string&)> gPick;
std::function<void(const std::string&)> gDeepLink;
std::string gPendingLink;
}  // namespace

void openUrl(const std::string& url) {
#if CC_TARGET_PLATFORM == CC_PLATFORM_ANDROID
    cocos2d::JniHelper::callStaticVoidMethod(kActivity, "openUrl", url);
#else
    cocos2d::Application::getInstance()->openURL(url);
#endif
}

void shareText(const std::string& text) {
#if CC_TARGET_PLATFORM == CC_PLATFORM_ANDROID
    cocos2d::JniHelper::callStaticVoidMethod(kActivity, "shareText", text);
#else
    CCLOG("share: %s", text.c_str());
#endif
}

void vibrate(int ms) {
#if CC_TARGET_PLATFORM == CC_PLATFORM_ANDROID
    cocos2d::JniHelper::callStaticVoidMethod(kActivity, "vibrate", ms);
#else
    (void)ms;
#endif
}

void showNotification(int id, const std::string& title, const std::string& body) {
#if CC_TARGET_PLATFORM == CC_PLATFORM_ANDROID
    cocos2d::JniHelper::callStaticVoidMethod(kActivity, "showNotification", id, title, body);
#else
    CCLOG("notification %d: %s — %s", id, title.c_str(), body.c_str());
#endif
}

void pickImage(std::function<void(const std::string&)> cb) {
    gPick = std::move(cb);
#if CC_TARGET_PLATFORM == CC_PLATFORM_ANDROID
    cocos2d::JniHelper::callStaticVoidMethod(kActivity, "pickImage");
#else
    deliverImagePicked("");
#endif
}

bool callsSupported() { return false; }

bool startCall(const std::string& livekitUrl, const std::string& token, bool video) {
#if CC_TARGET_PLATFORM == CC_PLATFORM_ANDROID
    return cocos2d::JniHelper::callStaticBooleanMethod(kActivity, "startCall", livekitUrl, token, video);
#else
    (void)livekitUrl; (void)token; (void)video;
    return false;
#endif
}

void setDeepLinkHandler(std::function<void(const std::string&)> cb) {
    gDeepLink = std::move(cb);
    if (gDeepLink && !gPendingLink.empty()) {
        std::string l = gPendingLink;
        gPendingLink.clear();
        gDeepLink(l);
    }
}

void deliverImagePicked(const std::string& path) {
    auto cb = std::move(gPick);
    gPick = nullptr;
    if (cb) cb(path);
}

void deliverDeepLink(const std::string& url) {
    if (gDeepLink) gDeepLink(url);
    else gPendingLink = url;
}

}  // namespace native
}  // namespace nx
