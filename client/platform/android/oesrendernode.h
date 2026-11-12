#ifndef OESRENDERNODE_H
#define OESRENDERNODE_H

#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <QOpenGLFunctions>
#include <QRectF>
#include <QSGRenderNode>

namespace Platform {

/// Renders an external texture in the Qt scene graph.
class OESRenderNode : public QSGRenderNode
{
public:
    /// Selects the external texture to render.
    void setTextureId(const GLuint texId)
    {
        m_texId = texId;
    }
    /// Sets the bounds used for rendering.
    void setRect(const QRectF &rect)
    {
        m_rect = rect;
    }

    /// Releases the GLES program and associated resources.
    void releaseResources() override;
    /// Draws the external texture into the scene graph.
    void render(const RenderState *state) override;
    /// Identifies the render states modified by this node.
    inline QSGRenderNode::StateFlags changedStates() const override
    {
        return BlendState | DepthState | StencilState | ScissorState;
    }
    /// Declares the node rendering capabilities.
    RenderingFlags flags() const override
    {
        return BoundedRectRendering;
    }
    /// Returns the node bounds.
    QRectF rect() const override
    {
        return m_rect;
    }

private:
    /// Creates the GLES program on first use.
    void ensureProgram();

    /// Render bounding rectangle.
    QRectF m_rect{};
    /// External OES texture ID.
    std::optional<GLuint> m_texId{};

    /// Shader program and vertex/uniform attribute handles.
    struct Data
    {
        /// Shader program ID.
        GLuint program = 0;
        /// Vertex position attribute location.
        GLuint posAttr = 0;
        /// Texture coordinate attribute location.
        GLuint texAttr = 0;
        /// Texture sampler uniform location.
        GLint texUniform = 0;
        /// Model-view-projection matrix uniform location.
        GLint mvpUniform = 0;
    };

    /// Active shader program data.
    std::optional<Data> m_program{};
};

} // namespace Platform

#endif // OESRENDERNODE_H
