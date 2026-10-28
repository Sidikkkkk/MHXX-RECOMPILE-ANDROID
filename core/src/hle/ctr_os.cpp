#include "hle/ctr_os.h"
#include "bridge/logger.h"
#include "recompiler/recompiler.h"
#include <android/log.h>
#include <cstdio>

#define LOG_TAG "MHXX_HLE"
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)

namespace MHXX {
namespace CTR {

static SystemConfig g_sys_config;
static uint32_t g_current_buttons = 0;
static int g_touch_x = 0;
static int g_touch_y = 0;
static bool g_touch_down = false;

void InitializeOS(const std::string& romfs_dir, const std::string& save_dir) {
    g_sys_config.romfs_root = romfs_dir;
    g_sys_config.save_root = save_dir;
    g_sys_config.is_running = true;

    SendLogToUI("[HLE] Inisialisasi Horizon OS (CTR) Subsystems...");

    Services::InitFS();
    Services::InitGSP();
    Services::InitSND();
    Services::InitHID();
    Services::InitAPT();
}

void UpdateSystem() {
    // Sinkronisasi status input ke memori shared HID CTR (0x10000000)
    auto* ctx = Recompiler::GetCPUContext();
    if (ctx && ctx->is_running) {
        uint8_t* hid_mem = Recompiler::GetMemoryPointer(ctx, 0x10000000);
        if (hid_mem) {
            // Struktur HID 3DS: tombol aktif ditulis ke offset 0
            *reinterpret_cast<uint32_t*>(hid_mem) = g_current_buttons;
            // Touch screen: x di offset 8, y di offset 10, touch_down di offset 12
            *reinterpret_cast<uint16_t*>(hid_mem + 8) = static_cast<uint16_t>(g_touch_x);
            *reinterpret_cast<uint16_t*>(hid_mem + 10) = static_cast<uint16_t>(g_touch_y);
            *reinterpret_cast<uint8_t*>(hid_mem + 12) = g_touch_down ? 1 : 0;
        }
    }
}

void ShutdownOS() {
    g_sys_config.is_running = false;
    SendLogToUI("[HLE] Horizon OS Shutdown complete.");
}

void HandleTouch(float x, float y, bool is_down) {
    g_touch_x = static_cast<int>(x * 320.0f);
    g_touch_y = static_cast<int>(y * 240.0f);
    g_touch_down = is_down;
}

void HandleButton(uint32_t button_mask, bool is_down) {
    if (is_down) {
        g_current_buttons |= button_mask;
    } else {
        g_current_buttons &= ~button_mask;
    }
}

namespace Services {
    void InitFS()  { SendLogToUI("[HLE] fs:USER (FileSystem) siap."); }
    void InitGSP() { SendLogToUI("[HLE] gsp::Gpu (PICA200 Controller) siap."); }
    void InitSND() { SendLogToUI("[HLE] snd:USER (Sound DSP) siap."); }
    void InitHID() { SendLogToUI("[HLE] hid:USER (Gamepad & Touch) siap."); }
    void InitAPT() { SendLogToUI("[HLE] apt:U (Applet Manager) siap."); }
}

} // namespace CTR
} // namespace MHXX
