#pragma once

#include <cstdint>

namespace MHXX {
namespace Graphics {

// PICA200 GPU Register Offsets (Nintendo 3DS DMP Maestro-2G GPU)
namespace PicaRegs {
    constexpr uint32_t TRIGGER_DRAW            = 0x0001;
    constexpr uint32_t TRIGGER_DRAW_INDEXED    = 0x0002;
    constexpr uint32_t VIEWPORT_WIDTH          = 0x0041;
    constexpr uint32_t VIEWPORT_HEIGHT         = 0x0042;
    
    // Framebuffer Color & Depth
    constexpr uint32_t COLORBUF_FORMAT         = 0x0110;
    constexpr uint32_t COLORBUF_ADDR           = 0x0111;
    constexpr uint32_t DEPTHBUF_FORMAT         = 0x0112;
    constexpr uint32_t DEPTHBUF_ADDR           = 0x0113;

    // Texture Units (0, 1, 2)
    constexpr uint32_t TEXTURE0_ADDR           = 0x0085;
    constexpr uint32_t TEXTURE0_FORMAT         = 0x008E;
    constexpr uint32_t TEXTURE1_ADDR           = 0x0095;
    constexpr uint32_t TEXTURE2_ADDR           = 0x00A5;

    // TEV (Texture Environment Combiner - 6 Stages)
    constexpr uint32_t TEV_STAGE0_COLOR_SRC    = 0x00C0;
    constexpr uint32_t TEV_STAGE0_COLOR_OP     = 0x00C1;
}

// Format Tekstur PICA200
enum class PicaTextureFormat : uint32_t {
    RGBA8       = 0x0,
    RGB8        = 0x1,
    RGBA5551    = 0x2,
    RGB565      = 0x3,
    RGBA4       = 0x4,
    ETC1        = 0x8,   // Format paling sering digunakan MHXX (Kompatibel hardware Adreno 610!)
    ETC1A4      = 0x9
};

// Pengaturan Profil Performa Khusus Perangkat (Snapdragon 680 / Adreno 610)
struct RenderConfig {
    enum class Backend {
        OpenGL_ES_3_2,  // Default untuk Adreno 610 (Stabil, hemat daya, bebas bug driver)
        Vulkan          // Alternatif untuk SoC kelas menengah/atas
    };

    Backend backend = Backend::OpenGL_ES_3_2;
    int resolution_scale = 1;        // 1 = 400x240 (Asli/Hemat), 2 = 800x480 (Tajam)
    bool enable_vsync = true;
    bool fast_etc1_passthrough = true; // Langsung lewat ke chip Adreno tanpa dekompresi CPU
    int target_fps = 60;
};

} // namespace Graphics
} // namespace MHXX

