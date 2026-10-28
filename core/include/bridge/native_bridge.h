#pragma once

#include <jni.h>

#ifdef __cplusplus
extern "C" {
#endif

JNIEXPORT jboolean JNICALL
Java_com_nativeport_mhxx_bridge_NativeBridge_initNativeEngine(
    JNIEnv* env,
    jobject thiz,
    jstring romfs_path,
    jstring save_path
);

JNIEXPORT void JNICALL
Java_com_nativeport_mhxx_bridge_NativeBridge_onSurfaceCreated(
    JNIEnv* env,
    jobject thiz,
    jint width,
    jint height
);

JNIEXPORT void JNICALL
Java_com_nativeport_mhxx_bridge_NativeBridge_onSurfaceChanged(
    JNIEnv* env,
    jobject thiz,
    jint width,
    jint height
);

JNIEXPORT void JNICALL
Java_com_nativeport_mhxx_bridge_NativeBridge_runFrame(
    JNIEnv* env,
    jobject thiz
);

JNIEXPORT void JNICALL
Java_com_nativeport_mhxx_bridge_NativeBridge_sendTouchInput(
    JNIEnv* env,
    jobject thiz,
    jfloat x,
    jfloat y,
    jboolean is_down
);

JNIEXPORT void JNICALL
Java_com_nativeport_mhxx_bridge_NativeBridge_sendButtonInput(
    JNIEnv* env,
    jobject thiz,
    jint button_mask,
    jboolean is_down
);

#ifdef __cplusplus
}
#endif

