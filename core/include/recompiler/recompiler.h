#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace MHXX {
namespace Recompiler {

struct CPUContext {
    uint32_t r[16];   // ARM r0 - r15 (r13 = SP, r14 = LR, r15 = PC)
    uint32_t cpsr;    // Current Program Status Register
    uint32_t fpscr;
    float    vfp[64]; // VFPv3 registers (s0-s31 / d0-d15)

    // Alokasi Virtual Address Space CTR
    std::vector<uint8_t> app_memory;  // 0x00100000 - 0x01000000 (15 MB biner text, rodata, data, bss)
    std::vector<uint8_t> fcram;       // 0x08000000 - 0x10000000 (128 MB Application Heap)
    std::vector<uint8_t> vram;        // 0x18000000 - 0x18600000 (6 MB PICA200 VRAM)
    std::vector<uint8_t> tls;         // 0x1FF80000 - 0x20000000 (512 KB TLS & IPC)

    bool is_running = false;
    bool is_initialized = false;
    uint64_t instruction_count = 0;
    uint64_t frame_count = 0;
    uint32_t current_pc = 0x00100000;
};

// Fungsi Akses Memori Virtual Guest
uint8_t* GetMemoryPointer(CPUContext* ctx, uint32_t addr);
uint32_t ReadMemory32(CPUContext* ctx, uint32_t addr);
void WriteMemory32(CPUContext* ctx, uint32_t addr, uint32_t val);
uint16_t ReadMemory16(CPUContext* ctx, uint32_t addr);
void WriteMemory16(CPUContext* ctx, uint32_t addr, uint16_t val);
uint8_t ReadMemory8(CPUContext* ctx, uint32_t addr);
void WriteMemory8(CPUContext* ctx, uint32_t addr, uint8_t val);

// Siklus Eksekusi Static Recompiler
void InitRuntime(const std::string& romfs_dir);
void ExecuteFrame();
void ShutdownRuntime();

// Dispatcher Eksekusi
void ExecuteInstruction(CPUContext* ctx);
void StepRecompiledBlock(CPUContext* ctx, uint32_t max_instructions);

CPUContext* GetCPUContext();

} // namespace Recompiler
} // namespace MHXX
