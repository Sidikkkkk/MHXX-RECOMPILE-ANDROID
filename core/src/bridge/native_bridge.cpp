#include "bridge/native_bridge.h"
#include "hle/ctr_os.h"
#include "recompiler/recompiler.h"
#include "graphics/gpu_backend.h"
#include <android/log.h>
#include <string>
#include <memory>

#define LOG_TAG "MHXX_NATIVE"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

static bool g_engine_initialized = false;
static std::unique_ptr<MHXX::Graphics::IGpuBackend> g_gpu_backend = nullptr;

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
Java_com_nativeport_mhxx_bridge_NativeBridge_onSurfaceCreated(
    JNIEnv* env,
    jobject thiz,
    jint width,
    jint height
) {
    int w = width > 0 ? width : 1280;
    int h = height > 0 ? height : 720;
    LOGI("Surface dibuat: %dx%d. Memulai GPU Renderer...", w, h);
    MHXX::Graphics::RenderConfig cfg;
    cfg.backend = MHXX::Graphics::RenderConfig::Backend::OpenGL_ES_3_2;
    cfg.resolution_scale = 1; // Default 1x untuk Snapdragon 680 (bisa diubah ke 2x di settings)
    cfg.fast_etc1_passthrough = true;
    cfg.enable_vsync = true;

    g_gpu_backend = MHXX::Graphics::CreateGpuBackend(cfg.backend);
    if (g_gpu_backend) {
        g_gpu_backend->Initialize(w, h, cfg);
        LOGI("GPU Backend aktif: %s", g_gpu_backend->GetBackendName());
    }
}

JNIEXPORT void JNICALL
Java_com_nativeport_mhxx_bridge_NativeBridge_onSurfaceChanged(
    JNIEnv* env,
    jobject thiz,
    jint width,
    jint height
) {
    if (g_gpu_backend && width > 0 && height > 0) {
        g_gpu_backend->Resize(width, height);
    }
}

JNIEXPORT void JNICALL
Java_com_nativeport_mhxx_bridge_NativeBridge_runFrame(
    JNIEnv* env,
    jobject thiz
) {
    if (!g_engine_initialized) return;

    if (g_gpu_backend) {
        g_gpu_backend->BeginFrame();
    }

    MHXX::CTR::UpdateSystem();
    MHXX::Recompiler::ExecuteFrame();

    if (g_gpu_backend) {
        g_gpu_backend->EndFrame();
    }
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

