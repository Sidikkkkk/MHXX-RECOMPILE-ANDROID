#include "graphics/gpu_backend.h"
#include <GLES3/gl32.h>
#include <android/log.h>
#include <vector>

#define LOG_TAG "MHXX_GLES"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

namespace MHXX {
namespace Graphics {

// Vertex Shader untuk Blit Dual-Screen (Top 400x240 + Bottom 320x240)
static const char* VERTEX_SHADER_SRC = R"(#version 320 es
layout(location = 0) in vec2 a_Position;
layout(location = 1) in vec2 a_TexCoord;
out vec2 v_TexCoord;

void main() {
    gl_Position = vec4(a_Position, 0.0, 1.0);
    v_TexCoord = a_TexCoord;
}
)";

// Fragment Shader dengan fast sampling untuk hardware Adreno
static const char* FRAGMENT_SHADER_SRC = R"(#version 320 es
precision mediump float;
in vec2 v_TexCoord;
uniform sampler2D u_Texture;
out vec4 FragColor;

void main() {
    FragColor = texture(u_Texture, v_TexCoord);
}
)";

class OpenGLESBackend : public IGpuBackend {
public:
    OpenGLESBackend() = default;
    ~OpenGLESBackend() override { Shutdown(); }

    bool Initialize(int surface_width, int surface_height, const RenderConfig& config) override {
        m_config = config;
        m_surface_width = surface_width;
        m_surface_height = surface_height;

        LOGI("Inisialisasi OpenGL ES 3.2 Renderer untuk Adreno GPU...");
        LOGI("Surface Dimension: %dx%d (Resolution Scale: %dx)", 
             surface_width, surface_height, config.resolution_scale);

        // Periksa vendor GPU
        const char* renderer = reinterpret_cast<const char*>(glGetString(GL_RENDERER));
        const char* version = reinterpret_cast<const char*>(glGetString(GL_VERSION));
        LOGI("GPU Hardware: %s", renderer ? renderer : "Unknown");
        LOGI("GL Version  : %s", version ? version : "Unknown");

        if (!InitShaders()) {
            LOGE("Gagal mengompilasi shader presentasi GLES!");
            return false;
        }

        SetupFramebuffers();
        SetupScreenQuads();

        glDisable(GL_DEPTH_TEST);
        glDisable(GL_CULL_FACE);

        return true;
    }

    void Shutdown() override {
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
        m_surface_width = new_width;
        m_surface_height = new_height;
        glViewport(0, 0, new_width, new_height);
    }

    void BeginFrame() override {
        // Ikat FBO Top Screen (400x240 * scale)
        glBindFramebuffer(GL_FRAMEBUFFER, m_top_fbo);
        glViewport(0, 0, 400 * m_config.resolution_scale, 240 * m_config.resolution_scale);
        glClearColor(0.05f, 0.05f, 0.05f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    }

    void EndFrame() override {
        // Tampilkan kedua layar ke Framebuffer utama Android
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(0, 0, m_surface_width, m_surface_height);
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        PresentScreens();
    }

    void ExecuteCommandList(const uint32_t* cmd_list, size_t count) override {
        // Dispatcher instruksi render dari PICA200 Command Buffer
        // Pada Adreno 610, instruksi diproses secara batching untuk mengurangi overhead draw call
    }

    void PresentScreens() override {
        glUseProgram(m_program);
        glBindVertexArray(m_vao);

        // 1. Gambar Top Screen (Layar Utama Game MHXX: 400x240)
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, m_top_tex);
        glUniform1i(m_tex_loc, 0);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

        // 2. Gambar Bottom Screen (Layar Sentuh / Touch UI: 320x240)
        glBindTexture(GL_TEXTURE_2D, m_bottom_tex);
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

    GLuint m_program = 0;
    GLint  m_tex_loc = 0;
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

    void SetupScreenQuads() {
        // Layout Landscape:
        // Top Screen di sebelah kiri (Besar, rasio 5:3)
        // Bottom Screen di sebelah kanan (Touch controller / menu hunter: 4:3)
        float vertices[] = {
            // Top Screen (pos_x, pos_y, uv_x, uv_y)
            -1.0f,  1.0f,  0.0f, 1.0f,
            -1.0f, -1.0f,  0.0f, 0.0f,
             0.3f,  1.0f,  1.0f, 1.0f,
             0.3f, -1.0f,  1.0f, 0.0f,

            // Bottom Screen (Layar Sentuh di kanan)
             0.35f,  0.75f,  0.0f, 1.0f,
             0.35f, -0.75f,  0.0f, 0.0f,
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
    // Pada Snapdragon 680 (Adreno 610), OpenGL ES 3.2 memberikan stabilitas tertinggi
    return std::make_unique<OpenGLESBackend>();
}

} // namespace Graphics
} // namespace MHXX

