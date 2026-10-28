#pragma once

#include <string>
#include <cstdint>

namespace MHXX {
namespace CTR {

struct SystemConfig {
    std::string romfs_root;
    std::string save_root;
    bool is_running;
};

void InitializeOS(const std::string& romfs_dir, const std::string& save_dir);
void UpdateSystem();
void ShutdownOS();

void HandleTouch(float x, float y, bool is_down);
void HandleButton(uint32_t button_mask, bool is_down);

// Service HLE Interfaces
namespace Services {
    void InitFS();
    void InitGSP();
    void InitSND();
    void InitHID();
    void InitAPT();
}

} // namespace CTR
} // namespace MHXX

