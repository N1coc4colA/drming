#include "oesrendernode.h"

#include <QDebug>

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

GLuint compileShader(const GLenum type, const char* src)
{
    const auto shader = glCreateShader(type);
    glShaderSource(shader, 1, &src, nullptr);
    glCompileShader(shader);

    GLint ok = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        // Should never happen, but just in case.
        [[unlikely]];

        char log[512];
        glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
        qWarning() << "OES shader compile failed:" << log;
    }

    return shader;
}
} // namespace

void OESRenderNode::ensureProgram()
{
    if (m_program) {
        [[likely]];
        return;
    }

    const auto vs = compileShader(GL_VERTEX_SHADER, kVertexShader);
    const auto fs = compileShader(GL_FRAGMENT_SHADER, kFragmentShader);

    m_program = glCreateProgram();
    glAttachShader(m_program, vs);
    glAttachShader(m_program, fs);
    glBindAttribLocation(m_program, 0, "aPosition");
    glBindAttribLocation(m_program, 1, "aTexCoord");
    glLinkProgram(m_program);

    GLint ok = 0;
    glGetProgramiv(m_program, GL_LINK_STATUS, &ok);
    if (!ok) {
        // No error should ever happen here, but just in case.
        [[unlikely]];
        char log[512];
        glGetProgramInfoLog(m_program, sizeof(log), nullptr, log);
        qWarning() << "OES program link failed:" << log;
    }

    glDeleteShader(vs);
    glDeleteShader(fs);

    m_posAttr = 0;
    m_texAttr = 1;
    m_texUniform = glGetUniformLocation(m_program, "uTexture");
    m_mvpUniform = glGetUniformLocation(m_program, "uMvp");
}

void OESRenderNode::render(const RenderState* state)
{
    if (m_texId == 0) {
        [[unlikely]];
        return;
    }

    ensureProgram();

    glDisable(GL_BLEND);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_STENCIL_TEST);

    glUseProgram(m_program);

    // Combine the node's transform with Qt's projection matrix for this render pass.
    const auto mvp = *state->projectionMatrix() * *matrix();

    const float x0 = m_rect.left(), y0 = m_rect.top();
    const float x1 = m_rect.right(), y1 = m_rect.bottom();
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
    glBindTexture(GL_TEXTURE_EXTERNAL_OES, m_texId);
    // Filtering/wrap params are per-texture-object state, set once when the
    // texture is created in FfmpegDecoder::updateTextureFromHardwareBuffer.
    // No need to re-set them on every render call.
    glUniform1i(m_texUniform, 0);
    glUniformMatrix4fv(m_mvpUniform, 1, GL_FALSE, mvp.constData());

    glVertexAttribPointer(m_posAttr, 2, GL_FLOAT, GL_FALSE, 0, verts);
    glVertexAttribPointer(m_texAttr, 2, GL_FLOAT, GL_FALSE, 0, texCoords);
    glEnableVertexAttribArray(m_posAttr);
    glEnableVertexAttribArray(m_texAttr);

    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    glDisableVertexAttribArray(m_posAttr);
    glDisableVertexAttribArray(m_texAttr);
    glBindTexture(GL_TEXTURE_EXTERNAL_OES, 0);
}
} // namespace Platform
