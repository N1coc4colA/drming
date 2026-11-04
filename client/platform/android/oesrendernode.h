#ifndef OESRENDERNODE_H
#define OESRENDERNODE_H

#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <QOpenGLFunctions>
#include <QRectF>
#include <QSGRenderNode>

namespace Platform {

class OESRenderNode : public QSGRenderNode
{
public:
    void setTextureId(const GLuint texId)
    {
        m_texId = texId;
    }
    void setRect(const QRectF &rect)
    {
        m_rect = rect;
    }

    void render(const RenderState *state) override;
    inline QSGRenderNode::StateFlags changedStates() const override
    {
        return BlendState | DepthState | StencilState | ScissorState;
    }
    RenderingFlags flags() const override
    {
        return BoundedRectRendering;
    }
    QRectF rect() const override
    {
        return m_rect;
    }

private:
    void ensureProgram();

    QRectF m_rect{};
    std::optional<GLuint> m_texId{};

    struct Data
    {
        GLuint program = 0;
        GLuint posAttr = 0;
        GLuint texAttr = 0;
        GLint texUniform = 0;
        GLint mvpUniform = 0;
    };

    std::optional<Data> m_program{};
};

} // namespace Platform

#endif // OESRENDERNODE_H
