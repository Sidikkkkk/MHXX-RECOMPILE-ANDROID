#pragma once

#include "graphics/pica_registers.h"
#include <memory>

namespace MHXX {
namespace Graphics {

class IGpuBackend {
public:
    virtual ~IGpuBackend() = default;

    virtual bool Initialize(int surface_width, int surface_height, const RenderConfig& config) = 0;
    virtual void Shutdown() = 0;

    virtual void Resize(int new_width, int new_height) = 0;
    virtual void BeginFrame() = 0;
    virtual void EndFrame() = 0;

    // Eksekusi PICA200 Command List (Koleksi instruksi render dari Capcom MT Framework)
    virtual void ExecuteCommandList(const uint32_t* cmd_list, size_t count) = 0;

    // Update Framebuffer dari VRAM 3DS ke Texture OpenGL ES 3.2
    virtual void UpdateTopFramebuffer(const void* pixels, int width, int height) = 0;
    virtual void UpdateBottomFramebuffer(const void* pixels, int width, int height) = 0;

    // Presentasi dual-screen (Top Screen 400x240 & Bottom Screen 320x240) ke layar Android
    virtual void PresentScreens() = 0;

    virtual const char* GetBackendName() const = 0;
};

// Global accessor untuk backend aktif
IGpuBackend* GetActiveGpuBackend();
void SetActiveGpuBackend(IGpuBackend* backend);

// Factory untuk membuat renderer optimal berdasarkan GPU perangkat
std::unique_ptr<IGpuBackend> CreateGpuBackend(RenderConfig::Backend type);

} // namespace Graphics
} // namespace MHXX
