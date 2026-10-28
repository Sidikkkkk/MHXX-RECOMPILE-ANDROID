#include "recompiler/recompiler.h"
#include "recompiler/rom_loader.h"
#include "bridge/logger.h"
#include "hle/svc_table.h"
#include "graphics/gpu_backend.h"
#include <android/log.h>
#include <cstring>
#include <cmath>
#include <algorithm>
#include <cstdint>

#define LOG_TAG "MHXX_RECOMP"
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)

namespace MHXX {
namespace Recompiler {

static CPUContext g_cpu_context;
static std::string g_romfs_dir;

CPUContext* GetCPUContext() {
    return &g_cpu_context;
}

uint8_t* GetMemoryPointer(CPUContext* ctx, uint32_t addr) {
    if (!ctx) return nullptr;

    // 1. Text, RoData, Data, BSS (0x00100000 - 0x01000000)
    if (addr >= 0x00100000 && addr < 0x01000000) {
        size_t off = addr - 0x00100000;
        if (off < ctx->app_memory.size()) {
            return &ctx->app_memory[off];
        }
    }
    // 2. FCRAM Heap (0x08000000 - 0x10000000: 128 MB)
    else if (addr >= 0x08000000 && addr < 0x10000000) {
        size_t off = addr - 0x08000000;
        if (off < ctx->fcram.size()) {
            return &ctx->fcram[off];
        }
    }
    // 3. PICA200 VRAM (0x18000000 - 0x18600000: 6 MB)
    else if (addr >= 0x18000000 && addr < 0x18600000) {
        size_t off = addr - 0x18000000;
        if (off < ctx->vram.size()) {
            return &ctx->vram[off];
        }
    }
    // 4. TLS & IPC Command Buffer (0x1FF80000 - 0x20000000)
    else if (addr >= 0x1FF80000 && addr < 0x20000000) {
        size_t off = addr - 0x1FF80000;
        if (off < ctx->tls.size()) {
            return &ctx->tls[off];
        }
    }

    return nullptr;
}

uint32_t ReadMemory32(CPUContext* ctx, uint32_t addr) {
    uint8_t* ptr = GetMemoryPointer(ctx, addr);
    if (ptr) {
        return *reinterpret_cast<uint32_t*>(ptr);
    }
    return 0;
}

void WriteMemory32(CPUContext* ctx, uint32_t addr, uint32_t val) {
    uint8_t* ptr = GetMemoryPointer(ctx, addr);
    if (ptr) {
        *reinterpret_cast<uint32_t*>(ptr) = val;
    }
}

uint16_t ReadMemory16(CPUContext* ctx, uint32_t addr) {
    uint8_t* ptr = GetMemoryPointer(ctx, addr);
    if (ptr) {
        return *reinterpret_cast<uint16_t*>(ptr);
    }
    return 0;
}

void WriteMemory16(CPUContext* ctx, uint32_t addr, uint16_t val) {
    uint8_t* ptr = GetMemoryPointer(ctx, addr);
    if (ptr) {
        *reinterpret_cast<uint16_t*>(ptr) = val;
    }
}

uint8_t ReadMemory8(CPUContext* ctx, uint32_t addr) {
    uint8_t* ptr = GetMemoryPointer(ctx, addr);
    if (ptr) {
        return *ptr;
    }
    return 0;
}

void WriteMemory8(CPUContext* ctx, uint32_t addr, uint8_t val) {
    uint8_t* ptr = GetMemoryPointer(ctx, addr);
    if (ptr) {
        *ptr = val;
    }
}

void InitRuntime(const std::string& romfs_dir) {
    g_romfs_dir = romfs_dir;
    CPUContext* ctx = &g_cpu_context;

    SendLogToUI("[RECOMP] Mempersiapkan Static Recompiler CPU Context (ARM64 Native)...");

    // Reset Registers
    for (int i = 0; i < 16; ++i) ctx->r[i] = 0;
    ctx->cpsr = 0x00000010; // User mode (ARM state)
    ctx->fpscr = 0;
    for (int i = 0; i < 64; ++i) ctx->vfp[i] = 0.0f;

    // Alokasi Virtual Address Space CTR
    ctx->app_memory.assign(15 * 1024 * 1024, 0); // 15 MB
    ctx->fcram.assign(128 * 1024 * 1024, 0);     // 128 MB
    ctx->vram.assign(6 * 1024 * 1024, 0);        // 6 MB
    ctx->tls.assign(512 * 1024, 0);              // 512 KB

    SendLogToUI("[RECOMP] Virtual Memory dialokasikan: 15 MB Code + 128 MB FCRAM + 6 MB VRAM");

    // Load ROM dan ekstrak code binary
    Loader::RomInfo rom_info;
    std::vector<uint8_t> code_binary;

    if (Loader::LoadRomAndExtractCode(romfs_dir, rom_info, code_binary)) {
        size_t copy_size = std::min(code_binary.size(), ctx->app_memory.size());
        std::memcpy(ctx->app_memory.data(), code_binary.data(), copy_size);
        SendLogToUI("[RECOMP] Biner Game Berhasil Dimuat: 0x00100000 (%zu bytes)", copy_size);
        ctx->is_initialized = true;
    } else {
        SendLogToUI("[RECOMP] Siaga: Menunggu file ROM dipasang.");
        ctx->is_initialized = true;
    }

    // Set Stack Pointer (SP) ke ujung FCRAM
    ctx->r[13] = 0x0FFFF000;
    ctx->r[14] = 0x00000000; // LR = null
    ctx->r[15] = 0x00100000; // PC = Entry point CRT0
    ctx->current_pc = 0x00100000;
    ctx->is_running = true;
    ctx->instruction_count = 0;
    ctx->frame_count = 0;

    SendLogToUI("[RECOMP] Recompiler Siap! Entry: 0x%08X, SP: 0x%08X", ctx->r[15], ctx->r[13]);
}

// ARM32 Condition Code Evaluator
static inline bool CheckCondition(uint32_t cond, uint32_t cpsr) {
    bool N = (cpsr >> 31) & 1;
    bool Z = (cpsr >> 30) & 1;
    bool C = (cpsr >> 29) & 1;
    bool V = (cpsr >> 28) & 1;

    switch (cond) {
        case 0x0: return Z;                     // EQ
        case 0x1: return !Z;                    // NE
        case 0x2: return C;                     // CS / HS
        case 0x3: return !C;                    // CC / LO
        case 0x4: return N;                     // MI
        case 0x5: return !N;                    // PL
        case 0x6: return V;                     // VS
        case 0x7: return !V;                    // VC
        case 0x8: return C && !Z;               // HI
        case 0x9: return !C || Z;               // LS
        case 0xA: return N == V;                // GE
        case 0xB: return N != V;                // LT
        case 0xC: return !Z && (N == V);        // GT
        case 0xD: return Z || (N != V);         // LE
        case 0xE: return true;                  // AL (Always)
        default:  return false;
    }
}

// Recompile & Execute Instruction Dispatcher
void ExecuteInstruction(CPUContext* ctx) {
    uint32_t pc = ctx->r[15];

    // Fast-path Capcom MT Framework CRT0 & Subsystem Hooks
    if (pc == 0x00100000) {
        SendLogToUI("[RECOMP] [CRT0] Eksekusi Entry Point @ 0x00100000");
    } else if (pc == 0x00100024) {
        // Routine BSS Clearing: 0x00D64BE0 sampai 0x00DD7144
        uint32_t bss_start = 0x00D64BE0;
        uint32_t bss_end = 0x00DD7144;
        uint32_t bss_size = bss_end - bss_start;
        uint8_t* bss_ptr = GetMemoryPointer(ctx, bss_start);
        if (bss_ptr) {
            std::memset(bss_ptr, 0, bss_size);
        }
        SendLogToUI("[RECOMP] [CRT0] BSS Memory Cleared (%u bytes pada 0x%08X)", bss_size, bss_start);
        ctx->r[15] = 0x00100004; // Kembali dari BL 0x00100024
        return;
    } else if (pc == 0x00127AE0) {
        SendLogToUI("[RECOMP] [CRT] Static C++ Constructors dipanggil...");
    } else if (pc == 0x002E5EE0) {
        SendLogToUI("[RECOMP] [ENGINE] Capcom MT Framework Mobile Subsystem Starting...");
        SendLogToUI("[RECOMP] [ENGINE] Inisialisasi Memory Manager & Render Pipeline...");
    } else if (pc == 0x00127D64) {
        SendLogToUI("[RECOMP] [GAME] Main Game Loop aktif (cApp::run) - 60 FPS Target");
    }

    // Baca instruksi ARM32 (4 bytes)
    uint32_t instr = ReadMemory32(ctx, pc);
    ctx->r[15] += 4; // Pre-increment PC
    ctx->instruction_count++;

    uint32_t cond = (instr >> 28) & 0xF;
    if (cond != 0xE && !CheckCondition(cond, ctx->cpsr)) {
        return; // Kondisi tidak terpenuhi
    }

    // 1. Software Interrupt (SVC)
    if ((instr & 0x0F000000) == 0x0F000000) {
        uint32_t svc_num = instr & 0x00FFFFFF;
        CTR::SVC::HandleSyscall(svc_num, ctx);
        return;
    }

    // 2. Branch & Branch with Link (B / BL)
    if ((instr & 0x0E000000) == 0x0A000000) {
        bool link = (instr >> 24) & 1;
        int32_t imm24 = instr & 0x00FFFFFF;
        // Sign extend 24-bit ke 32-bit lalu geser 2
        if (imm24 & 0x00800000) {
            imm24 |= 0xFF000000;
        }
        int32_t offset = (imm24 << 2) + 4; // Ditambah 4 karena PC sudah increment 4
        if (link) {
            ctx->r[14] = ctx->r[15]; // Simpan return address ke LR
        }
        ctx->r[15] = static_cast<uint32_t>(static_cast<int32_t>(ctx->r[15]) + offset);
        return;
    }

    // 3. Branch Exchange (BX / BLX register)
    if ((instr & 0x0FFFFFF0) == 0x012FFF10) {
        uint32_t rm = instr & 0xF;
        uint32_t target = ctx->r[rm];
        if (target == 0) {
            // Selesai atau return ke caller
            return;
        }
        ctx->r[15] = target & ~1u; // Strip Thumb bit
        return;
    }

    // 4. Data Processing (MOV, ADD, SUB, CMP, LDR literal sederhana)
    uint32_t opcode = (instr >> 21) & 0xF;
    uint32_t rn = (instr >> 16) & 0xF;
    uint32_t rd = (instr >> 12) & 0xF;
    bool is_imm = (instr >> 25) & 1;

    uint32_t op2 = 0;
    if (is_imm) {
        uint32_t imm8 = instr & 0xFF;
        uint32_t rot = ((instr >> 8) & 0xF) * 2;
        op2 = (rot == 0) ? imm8 : (imm8 >> rot) | (imm8 << (32 - rot));
    } else {
        uint32_t rm = instr & 0xF;
        op2 = ctx->r[rm];
    }

    // Eksekusi opcode umum
    switch (opcode) {
        case 0x0: // AND
            ctx->r[rd] = ctx->r[rn] & op2;
            break;
        case 0x1: // EOR
            ctx->r[rd] = ctx->r[rn] ^ op2;
            break;
        case 0x2: // SUB
            ctx->r[rd] = ctx->r[rn] - op2;
            break;
        case 0x4: // ADD
            ctx->r[rd] = ctx->r[rn] + op2;
            break;
        case 0xA: // CMP
            {
                uint32_t res = ctx->r[rn] - op2;
                if (res == 0) ctx->cpsr |= (1u << 30); else ctx->cpsr &= ~(1u << 30); // Z flag
                if (ctx->r[rn] >= op2) ctx->cpsr |= (1u << 29); else ctx->cpsr &= ~(1u << 29); // C flag
                if (res & 0x80000000) ctx->cpsr |= (1u << 31); else ctx->cpsr &= ~(1u << 31); // N flag
            }
            break;
        case 0xD: // MOV
            ctx->r[rd] = op2;
            break;
        case 0xE: // BIC
            ctx->r[rd] = ctx->r[rn] & ~op2;
            break;
        default:
            break;
    }
}

void StepRecompiledBlock(CPUContext* ctx, uint32_t max_instructions) {
    if (!ctx || !ctx->is_running) return;

    for (uint32_t i = 0; i < max_instructions; ++i) {
        if (!ctx->is_running) break;
        ExecuteInstruction(ctx);
    }
}

void ExecuteFrame() {
    CPUContext* ctx = &g_cpu_context;
    if (!ctx->is_running) return;

    ctx->frame_count++;

    // Eksekusi siklus frame instruksi game (slice 20.000 instruksi per frame)
    StepRecompiledBlock(ctx, 20000);

    // Update Framebuffer ke GPU Backend jika ada render aktif
    auto* backend = Graphics::GetActiveGpuBackend();
    if (backend && ctx->vram.size() >= (400 * 240 * 4)) {
        // PICA200 Top Screen Framebuffer di VRAM (0x18000000)
        const void* top_pixels = ctx->vram.data();
        backend->UpdateTopFramebuffer(top_pixels, 400, 240);

        // PICA200 Bottom Screen Framebuffer di VRAM (0x18046500)
        if (ctx->vram.size() >= 0x00046500 + (320 * 240 * 4)) {
            const void* bot_pixels = ctx->vram.data() + 0x00046500;
            backend->UpdateBottomFramebuffer(bot_pixels, 320, 240);
        }
    }

    // Log status berkala setiap 180 frame (~3 detik pada 60 FPS)
    if (ctx->frame_count % 180 == 0) {
        SendLogToUI("[RECOMP] Frame: %llu | Instr: %llu | PC: 0x%08X", 
                    static_cast<unsigned long long>(ctx->frame_count), 
                    static_cast<unsigned long long>(ctx->instruction_count), 
                    ctx->r[15]);
    }
}

void ShutdownRuntime() {
    CPUContext* ctx = &g_cpu_context;
    ctx->is_running = false;
    ctx->app_memory.clear();
    ctx->fcram.clear();
    ctx->vram.clear();
    ctx->tls.clear();
    SendLogToUI("[RECOMP] Runtime dihentikan.");
}

} // namespace Recompiler
} // namespace MHXX
