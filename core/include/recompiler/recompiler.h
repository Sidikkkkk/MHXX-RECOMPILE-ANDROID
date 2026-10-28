#pragma once

#include <cstdint>

namespace MHXX {
namespace Recompiler {

struct CPUContext {
    uint32_t r[16];   // ARM r0 - r15
    uint32_t cpsr;
    uint32_t fpscr;
    float    vfp[64]; // VFPv2 / VFPv3 registers
};

void InitRuntime();
void ExecuteFrame();
void CallRecompiledFunction(uint32_t target_address, CPUContext* ctx);

} // namespace Recompiler
} // namespace MHXX

