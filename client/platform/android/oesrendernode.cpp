#include "oesrendernode.h"

#include <QDebug>

#include <ctrack.hpp>

namespace Platform {
namespace {
constexpr auto kVertexShader = R"(
    attribute highp vec4 aPosition;
    attribute highp vec2 aTexCoord;
    uniform highp mat4 uMvp;
    varying highp vec2 vTexCoord;
    void main() {
        gl_Position = uMvp * aPosition;
        vTexCoord = aTexCoord;
    }
)";

constexpr auto kFragmentShader = R"(
    #extension GL_OES_EGL_image_external : require
    precision mediump float;
    uniform samplerExternalOES uTexture;
    varying highp vec2 vTexCoord;
    void main() {
        gl_FragColor = texture2D(uTexture, vTexCoord);
    }
)";

GLuint compileShader(const GLenum type, const char *src)
{
    const auto shader = glCreateShader(type);
    glShaderSource(shader, 1, &src, nullptr);
    glCompileShader(shader);

    GLint ok = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (!ok) [[unlikely]] {
        // Should never happen, but just in case.
        char log[512];
        glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
        qWarning() << "OES shader compile failed:" << log;
    }

    return shader;
}
} // namespace

void OESRenderNode::ensureProgram()
{
    if (m_program.has_value()) [[likely]] {
        return;
    }

    m_program = Data{};
    auto &prog = m_program.value();

    const auto vs = compileShader(GL_VERTEX_SHADER, kVertexShader);
    const auto fs = compileShader(GL_FRAGMENT_SHADER, kFragmentShader);

    prog.program = glCreateProgram();
    glAttachShader(prog.program, vs);
    glAttachShader(prog.program, fs);
    glBindAttribLocation(prog.program, 0, "aPosition");
    glBindAttribLocation(prog.program, 1, "aTexCoord");
    glLinkProgram(prog.program);

    GLint ok = 0;
    glGetProgramiv(prog.program, GL_LINK_STATUS, &ok);
    if (!ok) [[unlikely]] {
        // No error should ever happen here, but just in case.
        char log[512];
        glGetProgramInfoLog(prog.program, sizeof(log), nullptr, log);
        qWarning() << "OES program link failed:" << log;
    }

    glDeleteShader(vs);
    glDeleteShader(fs);

    prog.posAttr = 0;
    prog.texAttr = 1;
    prog.texUniform = glGetUniformLocation(prog.program, "uTexture");
    prog.mvpUniform = glGetUniformLocation(prog.program, "uMvp");
}

void OESRenderNode::render(const RenderState *state)
{
    if (!m_texId.has_value()) [[unlikely]] {
        return;
    }

    CTRACK;

    ensureProgram();

    auto &prog = m_program.value();

    glDisable(GL_BLEND);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_STENCIL_TEST);

    glUseProgram(prog.program);

    // Combine the node's transform with Qt's projection matrix for this render pass.
    const auto mvp = *state->projectionMatrix() * *matrix();

    const auto x0 = static_cast<float>(m_rect.left()), y0 = static_cast<float>(m_rect.top());
    const auto x1 = static_cast<float>(m_rect.right()), y1 = static_cast<float>(m_rect.bottom());
    const GLfloat verts[] = {
        x0,
        y0,
        x1,
        y0,
        x0,
        y1,
        x1,
        y1,
    };
    // MirrorVertically-equivalent: flip V so the OES source (bottom-left origin) displays right-side up.
    const GLfloat texCoords[] = {
        0.0f,
        0.0f,
        1.0f,
        0.0f,
        0.0f,
        1.0f,
        1.0f,
        1.0f,
    };

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_EXTERNAL_OES, m_texId.value());
    // Filtering/wrap params are per-texture-object state, set once when the
    // texture is created in VideoDecoder::updateTextureFromHardwareBuffer.
    // No need to re-set them on every render call.
    glUniform1i(prog.texUniform, 0);
    glUniformMatrix4fv(prog.mvpUniform, 1, GL_FALSE, mvp.constData());

    glVertexAttribPointer(prog.posAttr, 2, GL_FLOAT, GL_FALSE, 0, verts);
    glVertexAttribPointer(prog.texAttr, 2, GL_FLOAT, GL_FALSE, 0, texCoords);
    glEnableVertexAttribArray(prog.posAttr);
    glEnableVertexAttribArray(prog.texAttr);

    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    glDisableVertexAttribArray(prog.posAttr);
    glDisableVertexAttribArray(prog.texAttr);
    glBindTexture(GL_TEXTURE_EXTERNAL_OES, 0);
}

} // namespace Platform
