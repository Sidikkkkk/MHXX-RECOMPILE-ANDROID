#include "hle/ipc.h"
#include "recompiler/recompiler.h"
#include <android/log.h>

#define LOG_TAG "MHXX_IPC"
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)

namespace MHXX {
namespace CTR {
namespace IPC {

// Identifikasi Service Handles (Virtual Sessions)
enum ServiceType {
    SERVICE_UNKNOWN = 0,
    SERVICE_GSP_GPU,
    SERVICE_SND_USER,
    SERVICE_HID_USER,
    SERVICE_FS_USER,
    SERVICE_APT_U
};

void HandleGspGpuCommand(uint32_t* cmd_buffer, uint16_t cmd_id) {
    LOGD("GSP::Gpu Command ID: 0x%04X", cmd_id);
    auto* ctx = Recompiler::GetCPUContext();
    switch (cmd_id) {
        case 0x0001: // TriggerCmdReqQueue (Kirim perintah grafis ke GPU PICA200)
            if (ctx) ctx->has_new_frame = true;
            cmd_buffer[1] = 0; // Sukses (ResultCode)
            break;
        case 0x0005: // SetBufferSwap (Ganti buffer layar atas/bawah)
            if (ctx) ctx->has_new_frame = true;
            cmd_buffer[1] = 0;
            break;
        default:
            cmd_buffer[1] = 0;
            break;
    }
}

void HandleHidUserCommand(uint32_t* cmd_buffer, uint16_t cmd_id) {
    LOGD("HID::User Command ID: 0x%04X", cmd_id);
    // Mengembalikan handle memory event shared controller & touchscreen
    cmd_buffer[1] = 0;
}

void HandleSndUserCommand(uint32_t* cmd_buffer, uint16_t cmd_id) {
    LOGD("SND::User Command ID: 0x%04X", cmd_id);
    cmd_buffer[1] = 0;
}

void HandleFsUserCommand(uint32_t* cmd_buffer, uint16_t cmd_id) {
    LOGD("FS::User Command ID: 0x%04X", cmd_id);
    cmd_buffer[1] = 0;
}

void DispatchServiceSync(uint32_t session_handle, uint32_t* cmd_buffer) {
    if (!cmd_buffer) return;

    uint32_t header_raw = cmd_buffer[0];
    CommandHeader hdr = ParseHeader(header_raw);

    LOGD("Dispatch IPC: Handle=0x%08X, CmdID=0x%04X, Params=%u",
         session_handle, hdr.command_id, hdr.param_words);

    // Default mock dispatching berdasarkan session handle (atau port connection)
    switch (session_handle & 0xFF) {
        case 0x01:
            HandleGspGpuCommand(cmd_buffer, hdr.command_id);
            break;
        case 0x02:
            HandleHidUserCommand(cmd_buffer, hdr.command_id);
            break;
        case 0x03:
            HandleSndUserCommand(cmd_buffer, hdr.command_id);
            break;
        case 0x04:
            HandleFsUserCommand(cmd_buffer, hdr.command_id);
            break;
        default:
            // Sukseskan respon standar CTR
            cmd_buffer[1] = 0;
            break;
    }
}

} // namespace IPC
} // namespace CTR
} // namespace MHXX

