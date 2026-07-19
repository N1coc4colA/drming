#include "videoframeitem.h"

#include <QOpenGLContext>
#include <QQuickWindow>
#include <QSGSimpleTextureNode>

#include "ffmpeg.h"
#include "platform/android/oesrendernode.h"

namespace Platform {

QRectF fitKeepAspect(const QSizeF& itemSize, const QSizeF& imgSize)
{
    if (itemSize.isEmpty() || imgSize.isEmpty()) {
        return {};
    }

    const auto sx = itemSize.width() / imgSize.width();
    const auto sy = itemSize.height() / imgSize.height();
    const auto s = qMin(sx, sy);
    const auto w = imgSize.width() * s;
    const auto h = imgSize.height() * s;
    return {(itemSize.width() - w) * 0.5, (itemSize.height() - h) * 0.5, w, h};
}

VideoFrameItem::VideoFrameItem(QQuickItem* parent)
    : QQuickItem(parent)
{
    setFlag(ItemHasContents, true);
}

#ifdef USE_NAT_SURFACES
void VideoFrameItem::setDecoder(FfmpegDecoder* decoder)
{
    m_decoder = decoder;
    update();
}
#endif

void VideoFrameItem::setImage(const QImage& image)
{
    {
        QMutexLocker locker(&m_mtx);
        m_currentImage = image.convertToFormat(QImage::Format_RGBA8888);
        m_imageSize = m_currentImage.size();
        m_imageDirty = true;
    }
    update();
}

void VideoFrameItem::geometryChange(const QRectF& newGeometry, const QRectF& oldGeometry)
{
    QQuickItem::geometryChange(newGeometry, oldGeometry);

    if (newGeometry.size() != oldGeometry.size()) {
        update();
    }
}

QSGNode* VideoFrameItem::updatePaintNode(QSGNode* oldNode, UpdatePaintNodeData* data)
{
    Q_UNUSED(data);

#ifdef USE_NAT_SURFACES
    if (m_decoder) {
        auto node = static_cast<QSGNode*>(oldNode);
        if (!node) {
            [[unlikely]];

            node = new QSGNode();
        }

        m_decoder->consumeFrame();
        const auto texId = m_decoder->oesTextureId();
        if (texId != 0) {
            [[unlikely]];

            auto oesNode = node->childCount() ? static_cast<OESRenderNode*>(node->firstChild()) : nullptr;
            if (!oesNode) {
                [[unlikely]];

                oesNode = new OESRenderNode();
                node->removeAllChildNodes();
                node->appendChildNode(oesNode);
            }

            oesNode->setTextureId(texId);
            oesNode->setRect(fitKeepAspect(QSizeF(width(), height()), m_decoder->textureSize()));
            oesNode->markDirty(QSGNode::DirtyMaterial);
        }

        return node;
    } else {
#endif
        auto node = static_cast<QSGSimpleTextureNode*>(oldNode);
        if (!node) {
            [[unlikely]];

            node = new QSGSimpleTextureNode();
            node->setFiltering(QSGTexture::Linear);
        }

        bool textureChanged = false;
        QSize currentSize = m_imageSize;

        QImage localImage;
        bool dirty = false;
        {
            QMutexLocker locker(&m_mtx);
            if (m_imageDirty) {
                localImage = m_currentImage;
                m_imageDirty = false;
                dirty = true;
            }
        }

        if (dirty && !localImage.isNull()) {
            [[likely]];

            if (auto fallbackTex = window()->createTextureFromImage(localImage, QQuickWindow::TextureIsOpaque)) {
                [[likely]];

                fallbackTex->setFiltering(QSGTexture::Linear);
                node->setTexture(fallbackTex);
                node->setOwnsTexture(true);
                textureChanged = true;
                currentSize = localImage.size();
            }
        }

        // Always update geometry (aspect-fit) even if texture didn't change
        const QRectF rect = fitKeepAspect(size(), currentSize);
        node->setRect(rect);

        // Mark dirty if texture or geometry changed
        if (textureChanged || node->rect() != rect) {
            node->markDirty(QSGNode::DirtyMaterial | QSGNode::DirtyGeometry);
        }

        // Update m_imageSize for future checks
        m_imageSize = currentSize;

        return node;

#ifdef USE_NAT_SURFACES
    }
#endif
}

} // namespace Platform
