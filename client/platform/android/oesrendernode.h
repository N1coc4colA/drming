#ifndef OESRENDERNODE_H
#define OESRENDERNODE_H

#include <QOpenGLFunctions>
#include <QRectF>
#include <QSGRenderNode>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>

namespace Platform {

class OESRenderNode : public QSGRenderNode
{
public:
    void setTextureId(const GLuint texId) { m_texId = texId; }
    void setRect(const QRectF &rect) { m_rect = rect; }

    void render(const RenderState *state) override;
    inline QSGRenderNode::StateFlags changedStates() const override { return BlendState | DepthState | StencilState | ScissorState; }
    RenderingFlags flags() const override { return BoundedRectRendering; }
    QRectF rect() const override { return m_rect; }

private:
    void ensureProgram();

    GLuint m_texId = 0;
    QRectF m_rect;

    GLuint m_program = 0;
    GLint m_posAttr = -1;
    GLint m_texAttr = -1;
    GLint m_texUniform = -1;
    GLint m_mvpUniform = -1;
};

} // namespace Platform

#endif // OESRENDERNODE_H
