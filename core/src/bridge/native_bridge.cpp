#include "bridge/native_bridge.h"
#include "hle/ctr_os.h"
#include "recompiler/recompiler.h"
#include <android/log.h>
#include <string>

#define LOG_TAG "MHXX_NATIVE"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

static bool g_engine_initialized = false;

extern "C" {

JNIEXPORT jboolean JNICALL
Java_com_nativeport_mhxx_bridge_NativeBridge_initNativeEngine(
    JNIEnv* env,
    jobject thiz,
    jstring romfs_path,
    jstring save_path
) {
    const char* c_romfs = env->GetStringUTFChars(romfs_path, nullptr);
    const char* c_save = env->GetStringUTFChars(save_path, nullptr);

    LOGI("Inisialisasi MHXX Native Core...");
    LOGI("RomFS path: %s", c_romfs);
    LOGI("Save path: %s", c_save);

    // Inisialisasi OS layer pengganti CTR 3DS
    MHXX::CTR::InitializeOS(c_romfs, c_save);

    // Inisialisasi runtime static recompiler
    MHXX::Recompiler::InitRuntime();

    env->ReleaseStringUTFChars(romfs_path, c_romfs);
    env->ReleaseStringUTFChars(save_path, c_save);

    g_engine_initialized = true;
    return JNI_TRUE;
}

JNIEXPORT void JNICALL
Java_com_nativeport_mhxx_bridge_NativeBridge_runFrame(
    JNIEnv* env,
    jobject thiz
) {
    if (!g_engine_initialized) return;

    MHXX::CTR::UpdateSystem();
    MHXX::Recompiler::ExecuteFrame();
}

JNIEXPORT void JNICALL
Java_com_nativeport_mhxx_bridge_NativeBridge_sendTouchInput(
    JNIEnv* env,
    jobject thiz,
    jfloat x,
    jfloat y,
    jboolean is_down
) {
    MHXX::CTR::HandleTouch(x, y, is_down == JNI_TRUE);
}

JNIEXPORT void JNICALL
Java_com_nativeport_mhxx_bridge_NativeBridge_sendButtonInput(
    JNIEnv* env,
    jobject thiz,
    jint button_mask,
    jboolean is_down
) {
    MHXX::CTR::HandleButton(button_mask, is_down == JNI_TRUE);
}

}

