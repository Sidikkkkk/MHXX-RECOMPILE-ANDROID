#include "graphics/gpu_backend.h"
#include <GLES3/gl32.h>
#include <android/log.h>
#include <vector>
#include <memory>
#include <cmath>
#include <cstring>

#define LOG_TAG "MHXX_GLES"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

namespace MHXX {
namespace Graphics {

static IGpuBackend* g_active_backend = nullptr;

IGpuBackend* GetActiveGpuBackend() {
    return g_active_backend;
}

void SetActiveGpuBackend(IGpuBackend* backend) {
    g_active_backend = backend;
}

static const char* VERTEX_SHADER_SRC = R"(#version 320 es
layout(location = 0) in vec2 a_Position;
layout(location = 1) in vec2 a_TexCoord;
out vec2 v_TexCoord;

void main() {
    gl_Position = vec4(a_Position, 0.0, 1.0);
    v_TexCoord = a_TexCoord;
}
)";

static const char* FRAGMENT_SHADER_SRC = R"(#version 320 es
precision mediump float;
in vec2 v_TexCoord;
uniform sampler2D u_Texture;
uniform vec4 u_TintColor;
out vec4 FragColor;

void main() {
    vec4 tex = texture(u_Texture, v_TexCoord);
    FragColor = tex * u_TintColor;
}
)";

class OpenGLESBackend : public IGpuBackend {
public:
    OpenGLESBackend() = default;
    ~OpenGLESBackend() override { Shutdown(); }

    bool Initialize(int surface_width, int surface_height, const RenderConfig& config) override {
        m_config = config;
        m_surface_width = surface_width > 0 ? surface_width : 1920;
        m_surface_height = surface_height > 0 ? surface_height : 1080;

        LOGI("Inisialisasi OpenGL ES 3.2 Renderer untuk Adreno 610...");
        LOGI("Surface Dimension: %dx%d (Scale: %dx)", 
             m_surface_width, m_surface_height, config.resolution_scale);

        if (!InitShaders()) {
            LOGE("Gagal mengompilasi shader presentasi GLES!");
            return false;
        }

        SetupFramebuffers();
        GenerateBootScreens();
        SetupScreenQuads();

        glDisable(GL_DEPTH_TEST);
        glDisable(GL_CULL_FACE);

        SetActiveGpuBackend(this);
        LOGI("GPU Backend aktif: OpenGL ES 3.2");
        return true;
    }

    void Shutdown() override {
        if (g_active_backend == this) {
            SetActiveGpuBackend(nullptr);
        }
        if (m_program) {
            glDeleteProgram(m_program);
            m_program = 0;
        }
        if (m_top_fbo) {
            glDeleteFramebuffers(1, &m_top_fbo);
            glDeleteTextures(1, &m_top_tex);
        }
        if (m_bottom_fbo) {
            glDeleteFramebuffers(1, &m_bottom_fbo);
            glDeleteTextures(1, &m_bottom_tex);
        }
    }

    void Resize(int new_width, int new_height) override {
        if (new_width > 0 && new_height > 0) {
            m_surface_width = new_width;
            m_surface_height = new_height;
            glViewport(0, 0, new_width, new_height);
        }
    }

    void BeginFrame() override {
        m_frame_count++;
    }

    void EndFrame() override {
        // Bersihkan layar utama ke warna dark slate metal
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(0, 0, m_surface_width, m_surface_height);
        glClearColor(0.08f, 0.08f, 0.10f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        PresentScreens();
    }

    void ExecuteCommandList(const uint32_t* cmd_list, size_t count) override {
        // Eksekusi PICA200 render commands
    }

    void UpdateTopFramebuffer(const void* pixels, int width, int height) override {
        if (m_top_tex && pixels && width > 0 && height > 0) {
            glBindTexture(GL_TEXTURE_2D, m_top_tex);
            glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
        }
    }

    void UpdateBottomFramebuffer(const void* pixels, int width, int height) override {
        if (m_bottom_tex && pixels && width > 0 && height > 0) {
            glBindTexture(GL_TEXTURE_2D, m_bottom_tex);
            glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
        }
    }

    void PresentScreens() override {
        glUseProgram(m_program);
        glBindVertexArray(m_vao);

        // Efek pulse lembut pada HUD indikator bahwa engine aktif berjalan pada 60 FPS
        float pulse = 0.95f + 0.05f * std::sin(m_frame_count * 0.08f);

        // 1. Render Top Screen (Layar Utama Monster Hunter: 400x240)
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, m_top_tex);
        glUniform1i(m_tex_loc, 0);
        glUniform4f(m_tint_loc, pulse, pulse, pulse, 1.0f);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

        // 2. Render Bottom Screen (Layar Sentuh 3DS: 320x240)
        glBindTexture(GL_TEXTURE_2D, m_bottom_tex);
        glUniform4f(m_tint_loc, 1.0f, 1.0f, 1.0f, 1.0f);
        glDrawArrays(GL_TRIANGLE_STRIP, 4, 4);

        glBindVertexArray(0);
    }

    const char* GetBackendName() const override {
        return "OpenGL ES 3.2 (Optimized for Adreno 610)";
    }

private:
    RenderConfig m_config;
    int m_surface_width = 1920;
    int m_surface_height = 1080;
    uint64_t m_frame_count = 0;

    GLuint m_program = 0;
    GLint  m_tex_loc = 0;
    GLint  m_tint_loc = 0;
    GLuint m_vao = 0;
    GLuint m_vbo = 0;

    GLuint m_top_fbo = 0;
    GLuint m_top_tex = 0;
    GLuint m_bottom_fbo = 0;
    GLuint m_bottom_tex = 0;

    bool InitShaders() {
        GLuint vs = CompileShader(GL_VERTEX_SHADER, VERTEX_SHADER_SRC);
        GLuint fs = CompileShader(GL_FRAGMENT_SHADER, FRAGMENT_SHADER_SRC);
        if (!vs || !fs) return false;

        m_program = glCreateProgram();
        glAttachShader(m_program, vs);
        glAttachShader(m_program, fs);
        glLinkProgram(m_program);

        glDeleteShader(vs);
        glDeleteShader(fs);

        GLint linked = 0;
        glGetProgramiv(m_program, GL_LINK_STATUS, &linked);
        if (!linked) return false;

        m_tex_loc = glGetUniformLocation(m_program, "u_Texture");
        m_tint_loc = glGetUniformLocation(m_program, "u_TintColor");
        return true;
    }

    GLuint CompileShader(GLenum type, const char* src) {
        GLuint shader = glCreateShader(type);
        glShaderSource(shader, 1, &src, nullptr);
        glCompileShader(shader);
        GLint compiled = 0;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
        if (!compiled) {
            glDeleteShader(shader);
            return 0;
        }
        return shader;
    }

    void SetupFramebuffers() {
        int scale = m_config.resolution_scale;

        // Top Screen FBO (400x240)
        glGenFramebuffers(1, &m_top_fbo);
        glGenTextures(1, &m_top_tex);
        glBindTexture(GL_TEXTURE_2D, m_top_tex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 400 * scale, 240 * scale, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        glBindFramebuffer(GL_FRAMEBUFFER, m_top_fbo);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_top_tex, 0);

        // Bottom Screen FBO (320x240)
        glGenFramebuffers(1, &m_bottom_fbo);
        glGenTextures(1, &m_bottom_tex);
        glBindTexture(GL_TEXTURE_2D, m_bottom_tex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 320 * scale, 240 * scale, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        glBindFramebuffer(GL_FRAMEBUFFER, m_bottom_fbo);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_bottom_tex, 0);

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    void GenerateBootScreens() {
        // 1. Gambar Tampilan Top Screen (MHXX Title HUD & Status Display)
        const int top_w = 400 * m_config.resolution_scale;
        const int top_h = 240 * m_config.resolution_scale;
        std::vector<uint32_t> top_pixels(top_w * top_h);

        for (int y = 0; y < top_h; ++y) {
            for (int x = 0; x < top_w; ++x) {
                // Background gradien emas gelap khas Monster Hunter
                uint8_t r = static_cast<uint8_t>(25 + (y * 15) / top_h);
                uint8_t g = static_cast<uint8_t>(20 + (y * 10) / top_h);
                uint8_t b = static_cast<uint8_t>(15 + (x * 10) / top_w);

                // Border bingkai emas luar
                if (x < 6 || x >= top_w - 6 || y < 6 || y >= top_h - 6) {
                    r = 196; g = 132; b = 29; // Emas MH (#C4841D)
                }
                // Garis aksen tengah
                else if (y == top_h / 2 || y == top_h / 2 + 1) {
                    r = 80; g = 60; b = 30;
                }

                // Titik Lambang Hunter di tengah
                int cx = top_w / 2;
                int cy = top_h / 2;
                int dist_sq = (x - cx) * (x - cx) + (y - cy) * (y - cy);
                if (dist_sq < 2500 && dist_sq > 2200) {
                    r = 224; g = 169; b = 56;
                } else if (dist_sq <= 2200 && dist_sq >= 1800) {
                    r = 140; g = 30; b = 25; // Merah naga Double Cross
                }

                top_pixels[y * top_w + x] = 0xFF000000 | (b << 16) | (g << 8) | r;
            }
        }

        glBindTexture(GL_TEXTURE_2D, m_top_tex);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, top_w, top_h, GL_RGBA, GL_UNSIGNED_BYTE, top_pixels.data());

        // 2. Gambar Tampilan Bottom Screen (Panel Layar Sentuh 3DS: Target Cam & Palet Hunter)
        const int bot_w = 320 * m_config.resolution_scale;
        const int bot_h = 240 * m_config.resolution_scale;
        std::vector<uint32_t> bot_pixels(bot_w * bot_h);

        for (int y = 0; y < bot_h; ++y) {
            for (int x = 0; x < bot_w; ++x) {
                // Background dark iron grid
                uint8_t r = 30; uint8_t g = 32; uint8_t b = 38;

                // Border luar
                if (x < 5 || x >= bot_w - 5 || y < 5 || y >= bot_h - 5) {
                    r = 140; g = 140; b = 150;
                }
                // Garis pembagi grid touch screen (2x3 grid palet hunter)
                else if (x % (bot_w / 2) < 3 || y % (bot_h / 3) < 3) {
                    r = 60; g = 65; b = 75;
                }

                bot_pixels[y * bot_w + x] = 0xFF000000 | (b << 16) | (g << 8) | r;
            }
        }

        glBindTexture(GL_TEXTURE_2D, m_bottom_tex);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, bot_w, bot_h, GL_RGBA, GL_UNSIGNED_BYTE, bot_pixels.data());
    }

    void SetupScreenQuads() {
        // Tata Letak Landscape Android (Skala Layar 1080p):
        // Layar Atas (Top Screen 5:3) diletakkan di sisi kiri (-0.95 sampai 0.25)
        // Layar Bawah (Bottom Touch 4:3) diletakkan di sisi kanan (0.30 sampai 0.95)
        float vertices[] = {
            // Top Screen (pos_x, pos_y, uv_x, uv_y)
            -0.95f,  0.88f,  0.0f, 1.0f,
            -0.95f, -0.88f,  0.0f, 0.0f,
             0.25f,  0.88f,  1.0f, 1.0f,
             0.25f, -0.88f,  1.0f, 0.0f,

            // Bottom Screen (Layar Sentuh di kanan)
             0.30f,  0.75f,  0.0f, 1.0f,
             0.30f, -0.75f,  0.0f, 0.0f,
             0.95f,  0.75f,  1.0f, 1.0f,
             0.95f, -0.75f,  1.0f, 0.0f,
        };

        glGenVertexArrays(1, &m_vao);
        glGenBuffers(1, &m_vbo);

        glBindVertexArray(m_vao);
        glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
        glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), reinterpret_cast<void*>(0));

        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), reinterpret_cast<void*>(2 * sizeof(float)));

        glBindVertexArray(0);
    }
};

std::unique_ptr<IGpuBackend> CreateGpuBackend(RenderConfig::Backend type) {
    return std::make_unique<OpenGLESBackend>();
}

} // namespace Graphics
} // namespace MHXX
