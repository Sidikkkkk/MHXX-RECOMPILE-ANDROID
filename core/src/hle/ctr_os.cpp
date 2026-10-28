#include "hle/ctr_os.h"
#include <android/log.h>
#include <cstdio>

#define LOG_TAG "MHXX_HLE"
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)

namespace MHXX {
namespace CTR {

static SystemConfig g_sys_config;

void InitializeOS(const std::string& romfs_dir, const std::string& save_dir) {
    g_sys_config.romfs_root = romfs_dir;
    g_sys_config.save_root = save_dir;
    g_sys_config.is_running = true;

    LOGD("Inisialisasi Horizon OS HLE Subsystems di %s", romfs_dir.c_str());

    std::string game_bin = romfs_dir + "/game.bin";
    FILE* f = fopen(game_bin.c_str(), "rb");
    if (f) {
        fseek(f, 0, SEEK_END);
        long sz = ftell(f);
        fclose(f);
        LOGD("Ditemukan game.bin! Ukuran: %ld bytes (%.2f MB)", sz, sz / (1024.0 * 1024.0));
    }

    Services::InitFS();
    Services::InitGSP();
    Services::InitSND();
    Services::InitHID();
    Services::InitAPT();
}

void UpdateSystem() {
    // Dipanggil setiap frame dari main game loop
}

void ShutdownOS() {
    g_sys_config.is_running = false;
    LOGD("Horizon OS HLE Shutdown complete.");
}

void HandleTouch(float x, float y, bool is_down) {
    // Terjemahkan koordinat touch screen Android (0.0 - 1.0) ke resolusi 3DS bottom screen (320x240)
    int ctr_x = static_cast<int>(x * 320.0f);
    int ctr_y = static_cast<int>(y * 240.0f);
    // Masukkan ke register input buffer HID
}

void HandleButton(uint32_t button_mask, bool is_down) {
    // Mapping input tombol 3DS (A, B, X, Y, L, R, ZL, ZR, D-Pad, Circle Pad)
}

namespace Services {
    void InitFS()  { LOGD("FS (FileSystem) service initialized."); }
    void InitGSP() { LOGD("GSP (GPU display processor) service initialized."); }
    void InitSND() { LOGD("SND (Sound/DSP) service initialized."); }
    void InitHID() { LOGD("HID (Human Interface Device) service initialized."); }
    void InitAPT() { LOGD("APT (Applet) service initialized."); }
}

} // namespace CTR
} // namespace MHXX

