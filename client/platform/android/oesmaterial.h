#ifndef OESMATERIAL_H
#define OESMATERIAL_H

#include <QSGMaterial>

namespace Platform {

class OESMaterial : public QSGMaterial
{
public:
    OESMaterial() { setFlag(Blending, false); }

    QSGMaterialType* type() const override
    {
        static QSGMaterialType t;
        return &t;
    }

    QSGMaterialShader* createShader(QSGRendererInterface::RenderMode) const override;

private:
    QSGTexture* texture = nullptr;

    friend class OESMaterialShader;
};

class OESMaterialShader : public QSGMaterialShader
{
public:
    OESMaterialShader()
    {
        setShaderFileName(VertexStage, ":/shaders/oes.vert.qsb");
        setShaderFileName(FragmentStage, ":/shaders/oes.frag.qsb");
    }

    void updateSampledImage(RenderState& state, const int binding, QSGTexture** texture, QSGMaterial* newMaterial, QSGMaterial*) override
    {
        if (binding != 1) {
            return;
        }

        *texture = static_cast<OESMaterial*>(newMaterial)->texture;
    }
};

inline QSGMaterialShader* OESMaterial::createShader(const QSGRendererInterface::RenderMode) const
{
    return new OESMaterialShader;
}
} // namespace Platform

#endif // OESMATERIAL_H
