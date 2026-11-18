// JNI callbacks from com.nexora.cocos.AppActivity into C++ (run on the cocos GL thread).
#include <jni.h>

#include "platform/android/jni/JniHelper.h"
#include "platform/NativeBridge.h"

extern "C" {

JNIEXPORT void JNICALL Java_com_nexora_cocos_AppActivity_nativeOnImagePicked(JNIEnv* env, jclass, jstring path) {
    nx::native::deliverImagePicked(cocos2d::JniHelper::jstring2string(path));
}

JNIEXPORT void JNICALL Java_com_nexora_cocos_AppActivity_nativeOnDeepLink(JNIEnv* env, jclass, jstring url) {
    nx::native::deliverDeepLink(cocos2d::JniHelper::jstring2string(url));
}

}  // extern "C"
