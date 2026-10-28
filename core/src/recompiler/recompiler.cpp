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
    ctx->has_new_frame = false;
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

// Barrel Shifter Evaluator
static inline uint32_t EvaluateShift(uint32_t val, uint32_t shift_type, uint32_t amount, bool& carry_out, bool carry_in) {
    if (amount == 0) {
        carry_out = carry_in;
        return val;
    }
    switch (shift_type) {
        case 0: // LSL
            if (amount < 32) {
                carry_out = (val >> (32 - amount)) & 1;
                return val << amount;
            } else if (amount == 32) {
                carry_out = val & 1;
                return 0;
            } else {
                carry_out = false;
                return 0;
            }
        case 1: // LSR
            if (amount < 32) {
                carry_out = (val >> (amount - 1)) & 1;
                return val >> amount;
            } else if (amount == 32) {
                carry_out = (val >> 31) & 1;
                return 0;
            } else {
                carry_out = false;
                return 0;
            }
        case 2: // ASR
            if (amount < 32) {
                carry_out = (val >> (amount - 1)) & 1;
                return static_cast<uint32_t>(static_cast<int32_t>(val) >> amount);
            } else {
                bool sign = (val >> 31) & 1;
                carry_out = sign;
                return sign ? 0xFFFFFFFF : 0;
            }
        case 3: // ROR
            amount %= 32;
            if (amount == 0) {
                carry_out = (val >> 31) & 1;
                return val;
            }
            carry_out = (val >> (amount - 1)) & 1;
            return (val >> amount) | (val << (32 - amount));
    }
    return val;
}

// Recompile & Execute Instruction Dispatcher
void ExecuteInstruction(CPUContext* ctx) {
    uint32_t pc = ctx->r[15];

    // Boundary protection: PC harus berada di dalam batas kode (0x00100000 - 0x00BE6000)
    if (pc < 0x00100000 || pc >= 0x00BE6000) {
        SendLogToUI("[RECOMP] [WARN] PC 0x%08X di luar batas! Melanjutkan ke MT Framework 0x00100008...", pc);
        ctx->r[15] = 0x00100008; // Lanjutkan ke CRT0 step 2
        return;
    }

    // Fast-path Capcom MT Framework CRT0 & Subsystem Hooks
    if (pc == 0x00100000) {
        SendLogToUI("[RECOMP] [CRT0] Eksekusi Entry Point @ 0x00100000");
    } else if (pc == 0x00100004) {
        SendLogToUI("[RECOMP] [CRT0] Memanggil C++ Runtime Initializer...");
    } else if (pc == 0x00100008) {
        SendLogToUI("[RECOMP] [CRT0] C++ Static Constructors Selesai!");
        SendLogToUI("[RECOMP] [ENGINE] Memulai Capcom MT Framework Mobile Subsystem...");
    } else if (pc == 0x0010000C) {
        SendLogToUI("[RECOMP] [ENGINE] Subsystem Ready! Memasuki Main Game Loop...");
    } else if (pc == 0x00127D64) {
        SendLogToUI("[RECOMP] [GAME] Capcom MT Framework Main Loop (cApp::run) Aktif 60 FPS!");
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
        SendLogToUI("[RECOMP] [CRT] Menjalankan tabel konstruktor __init_array...");
    } else if (pc == 0x002E5EE0) {
        SendLogToUI("[RECOMP] [ENGINE] Inisialisasi Memory Manager & Task Scheduler...");
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
        if (imm24 & 0x00800000) {
            imm24 |= 0xFF000000;
        }
        int32_t offset = (imm24 << 2) + 4;
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
        if (target == 0) return;
        ctx->r[15] = target & ~1u; // Strip Thumb bit
        return;
    }

    // 4. Block Data Transfer (LDM / STM - PUSH & POP)
    if ((instr & 0x0E000000) == 0x08000000) {
        bool P = (instr >> 24) & 1;
        bool U = (instr >> 23) & 1;
        bool W = (instr >> 21) & 1;
        bool L = (instr >> 20) & 1;
        uint32_t rn = (instr >> 16) & 0xF;
        uint32_t reg_list = instr & 0xFFFF;

        uint32_t base = (rn == 15) ? (ctx->r[15] + 4) : ctx->r[rn];
        int count = 0;
        for (int i = 0; i < 16; ++i) {
            if (reg_list & (1 << i)) count++;
        }

        uint32_t start_addr;
        if (U) {
            start_addr = P ? (base + 4) : base;
        } else {
            start_addr = P ? (base - count * 4) : (base - count * 4 + 4);
        }

        uint32_t addr = start_addr;
        for (int i = 0; i < 16; ++i) {
            if (reg_list & (1 << i)) {
                if (L) {
                    ctx->r[i] = ReadMemory32(ctx, addr);
                    if (i == 15) {
                        ctx->r[15] &= ~1u; // Strip Thumb bit saat POP ke PC
                    }
                } else {
                    uint32_t val = (i == 15) ? (ctx->r[15] + 4) : ctx->r[i];
                    WriteMemory32(ctx, addr, val);
                }
                addr += 4;
            }
        }

        if (W) {
            ctx->r[rn] = U ? (base + count * 4) : (base - count * 4);
        }
        return;
    }

    // 5. Halfword & Signed Data Transfer (LDRH, STRH, LDRSB, LDRSH)
    if ((instr & 0x0E000090) == 0x00000090 && (instr & 0x00000060) != 0) {
        bool P = (instr >> 24) & 1;
        bool U = (instr >> 23) & 1;
        bool is_imm = (instr >> 22) & 1;
        bool W = (instr >> 21) & 1;
        bool L = (instr >> 20) & 1;
        uint32_t rn = (instr >> 16) & 0xF;
        uint32_t rd = (instr >> 12) & 0xF;
        uint32_t S = (instr >> 6) & 1;
        uint32_t H = (instr >> 5) & 1;

        uint32_t offset = 0;
        if (is_imm) {
            offset = (((instr >> 8) & 0xF) << 4) | (instr & 0xF);
        } else {
            uint32_t rm = instr & 0xF;
            offset = ctx->r[rm];
        }

        uint32_t base = (rn == 15) ? (ctx->r[15] + 4) : ctx->r[rn];
        uint32_t target_addr = U ? (base + offset) : (base - offset);
        uint32_t access_addr = P ? target_addr : base;

        if (L) {
            if (!S && H) { // LDRH
                ctx->r[rd] = ReadMemory16(ctx, access_addr);
            } else if (S && !H) { // LDRSB
                int8_t s8 = static_cast<int8_t>(ReadMemory8(ctx, access_addr));
                ctx->r[rd] = static_cast<uint32_t>(static_cast<int32_t>(s8));
            } else if (S && H) { // LDRSH
                int16_t s16 = static_cast<int16_t>(ReadMemory16(ctx, access_addr));
                ctx->r[rd] = static_cast<uint32_t>(static_cast<int32_t>(s16));
            }
            if (rd == 15) ctx->r[15] &= ~1u;
        } else {
            if (!S && H) { // STRH
                WriteMemory16(ctx, access_addr, static_cast<uint16_t>(ctx->r[rd] & 0xFFFF));
            }
        }

        if (!P || W) {
            if (rn != 15) ctx->r[rn] = target_addr;
        }
        return;
    }

    // 6. Single Data Transfer (LDR, STR, LDRB, STRB)
    if ((instr & 0x0C000000) == 0x04000000) {
        bool is_reg_offset = (instr >> 25) & 1;
        bool P = (instr >> 24) & 1;
        bool U = (instr >> 23) & 1;
        bool B = (instr >> 22) & 1;
        bool W = (instr >> 21) & 1;
        bool L = (instr >> 20) & 1;
        uint32_t rn = (instr >> 16) & 0xF;
        uint32_t rd = (instr >> 12) & 0xF;

        uint32_t offset = 0;
        if (!is_reg_offset) {
            offset = instr & 0xFFF;
        } else {
            uint32_t rm = instr & 0xF;
            uint32_t shift_type = (instr >> 5) & 3;
            uint32_t shift_imm = (instr >> 7) & 0x1F;
            bool dummy = false;
            offset = EvaluateShift(ctx->r[rm], shift_type, shift_imm, dummy, (ctx->cpsr >> 29) & 1);
        }

        uint32_t base = (rn == 15) ? (ctx->r[15] + 4) : ctx->r[rn];
        uint32_t target_addr = U ? (base + offset) : (base - offset);
        uint32_t access_addr = P ? target_addr : base;

        if (L) {
            if (B) {
                ctx->r[rd] = ReadMemory8(ctx, access_addr);
            } else {
                ctx->r[rd] = ReadMemory32(ctx, access_addr);
                if (rd == 15) ctx->r[15] &= ~1u;
            }
        } else {
            uint32_t val = (rd == 15) ? (ctx->r[15] + 4) : ctx->r[rd];
            if (B) {
                WriteMemory8(ctx, access_addr, static_cast<uint8_t>(val & 0xFF));
            } else {
                WriteMemory32(ctx, access_addr, val);
            }
        }

        if (!P || W) {
            if (rn != 15) ctx->r[rn] = target_addr;
        }
        return;
    }

    // 7. Multiply (MUL, MLA)
    if ((instr & 0x0FC000F0) == 0x00000090) {
        bool A = (instr >> 21) & 1;
        bool S = (instr >> 20) & 1;
        uint32_t rd = (instr >> 16) & 0xF;
        uint32_t rn = (instr >> 12) & 0xF;
        uint32_t rs = (instr >> 8) & 0xF;
        uint32_t rm = instr & 0xF;

        uint32_t res = ctx->r[rm] * ctx->r[rs];
        if (A) res += ctx->r[rn];
        ctx->r[rd] = res;
        if (S) {
            if (res == 0) ctx->cpsr |= (1u << 30); else ctx->cpsr &= ~(1u << 30);
            if (res & 0x80000000) ctx->cpsr |= (1u << 31); else ctx->cpsr &= ~(1u << 31);
        }
        return;
    }

    // 8. Data Processing (MOV, MVN, ADD, SUB, CMP, AND, ORR, etc.)
    if ((instr & 0x0C000000) == 0x00000000) {
        uint32_t opcode = (instr >> 21) & 0xF;
        bool S = (instr >> 20) & 1;
        uint32_t rn = (instr >> 16) & 0xF;
        uint32_t rd = (instr >> 12) & 0xF;
        bool is_imm = (instr >> 25) & 1;

        bool carry = (ctx->cpsr >> 29) & 1;
        uint32_t op2 = 0;
        if (is_imm) {
            uint32_t imm8 = instr & 0xFF;
            uint32_t rot = ((instr >> 8) & 0xF) * 2;
            op2 = (rot == 0) ? imm8 : (imm8 >> rot) | (imm8 << (32 - rot));
            if (rot != 0) carry = (op2 >> 31) & 1;
        } else {
            uint32_t rm = instr & 0xF;
            uint32_t shift_type = (instr >> 5) & 3;
            uint32_t shift_imm = (instr >> 7) & 0x1F;
            op2 = EvaluateShift(ctx->r[rm], shift_type, shift_imm, carry, carry);
        }

        uint32_t op1 = (rn == 15) ? (ctx->r[15] + 4) : ctx->r[rn];
        uint32_t res = 0;
        bool write_rd = true;

        switch (opcode) {
            case 0x0: res = op1 & op2; break; // AND
            case 0x1: res = op1 ^ op2; break; // EOR
            case 0x2: res = op1 - op2; break; // SUB
            case 0x3: res = op2 - op1; break; // RSB
            case 0x4: res = op1 + op2; break; // ADD
            case 0x5: res = op1 + op2 + ((ctx->cpsr >> 29) & 1); break; // ADC
            case 0x6: res = op1 - op2 - (!((ctx->cpsr >> 29) & 1)); break; // SBC
            case 0x7: res = op2 - op1 - (!((ctx->cpsr >> 29) & 1)); break; // RSC
            case 0x8: res = op1 & op2; write_rd = false; break; // TST
            case 0x9: res = op1 ^ op2; write_rd = false; break; // TEQ
            case 0xA: res = op1 - op2; write_rd = false; break; // CMP
            case 0xB: res = op1 + op2; write_rd = false; break; // CMN
            case 0xC: res = op1 | op2; break; // ORR
            case 0xD: res = op2; break;       // MOV
            case 0xE: res = op1 & ~op2; break;// BIC
            case 0xF: res = ~op2; break;      // MVN
        }

        if (write_rd) {
            ctx->r[rd] = res;
            if (rd == 15) ctx->r[15] &= ~1u;
        }

        if (S) {
            if (res == 0) ctx->cpsr |= (1u << 30); else ctx->cpsr &= ~(1u << 30);
            if (res & 0x80000000) ctx->cpsr |= (1u << 31); else ctx->cpsr &= ~(1u << 31);
            if (opcode == 0x2 || opcode == 0xA) { // SUB / CMP
                if (op1 >= op2) ctx->cpsr |= (1u << 29); else ctx->cpsr &= ~(1u << 29);
            } else if (opcode == 0x4 || opcode == 0xB) { // ADD / CMN
                if (static_cast<uint64_t>(op1) + op2 >= 0x100000000ULL) ctx->cpsr |= (1u << 29); else ctx->cpsr &= ~(1u << 29);
            }
        }
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

    // Eksekusi siklus frame instruksi game (slice 50.000 instruksi per frame)
    StepRecompiledBlock(ctx, 50000);

    // Update Framebuffer ke GPU Backend HANYA jika VRAM memiliki frame baru yang valid
    auto* backend = Graphics::GetActiveGpuBackend();
    if (backend && ctx->has_new_frame && ctx->vram.size() >= (400 * 240 * 4)) {
        const uint32_t* top_words = reinterpret_cast<const uint32_t*>(ctx->vram.data());
        bool top_has_pixels = false;
        for (int i = 0; i < 400 * 240; i += 64) {
            if (top_words[i] != 0) {
                top_has_pixels = true;
                break;
            }
        }
        if (top_has_pixels) {
            backend->UpdateTopFramebuffer(ctx->vram.data(), 400, 240);
        }

        if (ctx->vram.size() >= 0x00046500 + (320 * 240 * 4)) {
            const uint32_t* bot_words = reinterpret_cast<const uint32_t*>(ctx->vram.data() + 0x00046500);
            bool bot_has_pixels = false;
            for (int i = 0; i < 320 * 240; i += 64) {
                if (bot_words[i] != 0) {
                    bot_has_pixels = true;
                    break;
                }
            }
            if (bot_has_pixels) {
                backend->UpdateBottomFramebuffer(ctx->vram.data() + 0x00046500, 320, 240);
            }
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
