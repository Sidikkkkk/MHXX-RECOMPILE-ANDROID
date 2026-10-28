#include "hle/svc_table.h"
#include "hle/ipc.h"
#include "recompiler/recompiler.h"
#include <android/log.h>
#include <chrono>
#include <thread>
#include <cstring>

#define LOG_TAG "MHXX_SVC"
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__)

namespace MHXX {
namespace CTR {
namespace SVC {

// 3DS Hardware Clock Rate: 268.123480 MHz (SYSCLOCK)
static const uint64_t CTR_SYSCLOCK_HZ = 268123480;

static uint64_t GetCurrentSystemTick() {
    using namespace std::chrono;
    static auto start_time = high_resolution_clock::now();
    auto elapsed_ns = duration_cast<nanoseconds>(high_resolution_clock::now() - start_time).count();
    // Konversi nanoseconds ke CTR System Ticks
    return static_cast<uint64_t>((elapsed_ns * CTR_SYSCLOCK_HZ) / 1000000000ULL);
}

void HandleSyscall(uint32_t svc_id, void* cpu_ctx_ptr) {
    auto* ctx = static_cast<MHXX::Recompiler::CPUContext*>(cpu_ctx_ptr);

    switch (static_cast<SyscallID>(svc_id)) {
        case SVC_GetSystemTick: {
            // Mengembalikan nilai 64-bit tick counter ke r0 (low) dan r1 (high)
            uint64_t tick = GetCurrentSystemTick();
            ctx->r[0] = static_cast<uint32_t>(tick & 0xFFFFFFFF);
            ctx->r[1] = static_cast<uint32_t>((tick >> 32) & 0xFFFFFFFF);
            break;
        }

        case SVC_SleepThread: {
            // r0: Durasi sleep dalam nanosecond
            int64_t ns = static_cast<int64_t>(ctx->r[0]);
            if (ns > 0) {
                std::this_thread::sleep_for(std::chrono::nanoseconds(ns));
            } else {
                std::this_thread::yield();
            }
            ctx->r[0] = 0; // Sukses (RESULT_SUCCESS)
            break;
        }

        case SVC_SendSyncRequest: {
            // SVC Paling Krusial: Komunikasi IPC antar modul game dan Horizon OS
            // r0: Session Handle (gsp::Gpu, snd:USER, hid:USER, fs:USER)
            uint32_t session_handle = ctx->r[0];
            // TLS Command Buffer pada thread aktif dalam memori virtual CTR
            uint8_t* tls_ptr = MHXX::Recompiler::GetMemoryPointer(ctx, 0x1FF80080);
            uint32_t* cmd_buffer = reinterpret_cast<uint32_t*>(tls_ptr);

            IPC::DispatchServiceSync(session_handle, cmd_buffer);
            ctx->r[0] = 0; // RESULT_SUCCESS
            break;
        }

        case SVC_CloseHandle: {
            // Menutup handle event/mutex/port
            uint32_t handle = ctx->r[0];
            LOGD("SVC_CloseHandle: 0x%08X", handle);
            ctx->r[0] = 0; // RESULT_SUCCESS
            break;
        }

        case SVC_WaitSynchronization1: {
            // Sinkronisasi thread (menunggu mutex atau event)
            ctx->r[0] = 0; // RESULT_SUCCESS
            break;
        }

        case SVC_SignalEvent:
        case SVC_ClearEvent:
        case SVC_ResetEvent: {
            ctx->r[0] = 0; // RESULT_SUCCESS
            break;
        }

        case SVC_ControlMemory: {
            // Alokasi memori heap game
            uint32_t addr_out = ctx->r[1];
            uint32_t size = ctx->r[3];
            LOGD("SVC_ControlMemory: size = %u bytes", size);
            ctx->r[0] = 0; // RESULT_SUCCESS
            ctx->r[1] = addr_out; // Pointer memori yang dialokasikan
            break;
        }

        case SVC_ExitProcess: {
            LOGW("Game memanggil SVC_ExitProcess.");
            break;
        }

        default:
            LOGD("SVC [0x%02X] dipanggil.", svc_id);
            ctx->r[0] = 0;
            break;
    }
}

} // namespace SVC
} // namespace CTR
} // namespace MHXX

