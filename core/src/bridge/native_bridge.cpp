#include "bridge/native_bridge.h"
#include "bridge/logger.h"
#include "hle/ctr_os.h"
#include "recompiler/recompiler.h"
#include "graphics/gpu_backend.h"
#include <android/log.h>
#include <string>
#include <memory>
#include <cstdarg>

#define LOG_TAG "MHXX_NATIVE"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

static bool g_engine_initialized = false;
static std::unique_ptr<MHXX::Graphics::IGpuBackend> g_gpu_backend = nullptr;

static JavaVM* g_jvm = nullptr;
static jclass g_bridge_class = nullptr;
static jmethodID g_post_log_method = nullptr;

extern "C" {

JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* vm, void* reserved) {
    g_jvm = vm;
    JNIEnv* env = nullptr;
    if (vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) != JNI_OK) {
        return JNI_ERR;
    }
    jclass localClass = env->FindClass("com/nativeport/mhxx/bridge/NativeBridge");
    if (localClass) {
        g_bridge_class = reinterpret_cast<jclass>(env->NewGlobalRef(localClass));
        g_post_log_method = env->GetStaticMethodID(g_bridge_class, "postNativeLog", "(Ljava/lang/String;)V");
    }
    return JNI_VERSION_1_6;
}

void SendLogToUI(const char* fmt, ...) {
    char buffer[512];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);

    __android_log_write(ANDROID_LOG_INFO, LOG_TAG, buffer);

    if (g_jvm && g_bridge_class && g_post_log_method) {
        JNIEnv* env = nullptr;
        bool attached = false;
        if (g_jvm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) != JNI_OK) {
            if (g_jvm->AttachCurrentThread(&env, nullptr) == JNI_OK) {
                attached = true;
            }
        }
        if (env) {
            jstring jmsg = env->NewStringUTF(buffer);
            env->CallStaticVoidMethod(g_bridge_class, g_post_log_method, jmsg);
            env->DeleteLocalRef(jmsg);
            if (attached) {
                g_jvm->DetachCurrentThread();
            }
        }
    }
}

JNIEXPORT jboolean JNICALL
Java_com_nativeport_mhxx_bridge_NativeBridge_initNativeEngine(
    JNIEnv* env,
    jobject thiz,
    jstring romfs_path,
    jstring save_path
) {
    const char* c_romfs = env->GetStringUTFChars(romfs_path, nullptr);
    const char* c_save = env->GetStringUTFChars(save_path, nullptr);

    SendLogToUI("[CORE] Inisialisasi MHXX Native Core (ARM64)...");
    SendLogToUI("[FS] Jalur Penyimpanan: %s", c_romfs);

    // Inisialisasi OS layer pengganti CTR 3DS
    MHXX::CTR::InitializeOS(c_romfs, c_save);

    // Inisialisasi runtime static recompiler
    MHXX::Recompiler::InitRuntime(c_romfs);

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
    SendLogToUI("[GPU] Surface Dibuat: %dx%d. Inisialisasi Adreno 610 GLES 3.2...", w, h);

    MHXX::Graphics::RenderConfig cfg;
    cfg.backend = MHXX::Graphics::RenderConfig::Backend::OpenGL_ES_3_2;
    cfg.resolution_scale = 1;
    cfg.fast_etc1_passthrough = true;
    cfg.enable_vsync = true;

    g_gpu_backend = MHXX::Graphics::CreateGpuBackend(cfg.backend);
    if (g_gpu_backend) {
        g_gpu_backend->Initialize(w, h, cfg);
        SendLogToUI("[GPU] %s Aktif (Dual-Screen 60 FPS)", g_gpu_backend->GetBackendName());
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
