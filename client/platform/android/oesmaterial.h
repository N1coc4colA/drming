#ifndef OESMATERIAL_H
#define OESMATERIAL_H

#include <QSGMaterial>

namespace Platform {

/// Renders a decoded external texture.
class OESMaterial : public QSGMaterial
{
public:
    OESMaterial()
    {
        setFlag(Blending, false);
    }

    /// Returns the material type used by Qt Quick.
    QSGMaterialType *type() const override
    {
        static QSGMaterialType t;
        return &t;
    }

    /// Creates the material shader implementation.
    QSGMaterialShader *createShader(QSGRendererInterface::RenderMode) const override;

private:
    /// External texture referenced by the material.
    QSGTexture *texture = nullptr;

    friend class OESMaterialShader;
};

/// Defines the shader used by the external-texture material.
class OESMaterialShader : public QSGMaterialShader
{
public:
    OESMaterialShader()
    {
        setShaderFileName(VertexStage, ":/shaders/oes.vert.qsb");
        setShaderFileName(FragmentStage, ":/shaders/oes.frag.qsb");
    }

    /// Updates the image sampled by the material.
    void updateSampledImage(
        RenderState &,
        const int binding,
        QSGTexture **texture,
        QSGMaterial *newMaterial,
        QSGMaterial *
    ) override
    {
        if (binding != 1) {
            return;
        }

        *texture = static_cast<OESMaterial *>(newMaterial)->texture;
    }
};

inline QSGMaterialShader *OESMaterial::createShader(const QSGRendererInterface::RenderMode) const
{
    return new OESMaterialShader;
}
} // namespace Platform

#endif // OESMATERIAL_H
