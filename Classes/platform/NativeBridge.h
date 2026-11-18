// Platform services (Flutter plugins in the Dart app): image_picker, flutter_local_notifications, haptics,
// url_launcher/share, app updater download, LiveKit calls. Android goes through JNI to AppActivity.java;
// other platforms log and return "unsupported".
#pragma once
#include <functional>
#include <string>

namespace nx {
namespace native {

void openUrl(const std::string& url);
void shareText(const std::string& text);
void vibrate(int ms);
void showNotification(int id, const std::string& title, const std::string& body);
/// Opens the system picker; [cb] gets a local file path ("" = cancelled / failed).
void pickImage(std::function<void(const std::string& path)> cb);
/// LiveKit voice/video. Returns false when unsupported (current state; see docs/PORTING.md).
bool startCall(const std::string& livekitUrl, const std::string& token, bool video);
bool callsSupported();

/// Deep links (nexora://login-callback?code=...) arriving from the OS.
void setDeepLinkHandler(std::function<void(const std::string& url)> cb);

// Called from JNI.
void deliverImagePicked(const std::string& path);
void deliverDeepLink(const std::string& url);

}  // namespace native
}  // namespace nx
