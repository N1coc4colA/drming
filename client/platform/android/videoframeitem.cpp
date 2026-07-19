#include "videoframeitem.h"

#include <QOpenGLContext>
#include <QQuickWindow>
#include <QSGSimpleTextureNode>

#include "ffmpeg.h"
#include "oesrendernode.h"

namespace Platform {

QRectF fitKeepAspect(const QSizeF& itemSize, const QSizeF& imgSize)
{
    if (itemSize.isEmpty() || imgSize.isEmpty()) {
        [[unlikely]];

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

void VideoFrameItem::setDecoder(FfmpegDecoder* decoder)
{
    m_decoder = decoder;

    if (decoder) {
        if (m_textureMode != TextureMode::OES) {
            [[unlikely]];

            m_previousTextureMode = m_textureMode;
            m_textureMode = TextureMode::OES;
        }

        update();
    }
}

void VideoFrameItem::setImage(const QImage& image)
{
    {
        QMutexLocker locker(&m_mtx);
        m_currentImage = image.convertToFormat(QImage::Format_RGBA8888);
        m_imageSize = m_currentImage.size();
        m_imageDirty = true;
    }

    if (m_textureMode != TextureMode::Simple) {
        [[unlikely]];

        m_previousTextureMode = m_textureMode;
        m_textureMode = TextureMode::Simple;
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

    switch (m_textureMode) {
    case TextureMode::OES: {
        if (m_decoder) {
            auto node = static_cast<QSGNode*>(oldNode);
            if (m_textureMode != m_previousTextureMode && node) {
                [[unlikely]];

                delete node;
                node = nullptr;
            }

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
        }

        break;
    }
    case TextureMode::Simple: {
        auto node = static_cast<QSGSimpleTextureNode*>(oldNode);
        if (m_textureMode != m_previousTextureMode && node) {
            [[unlikely]];

            delete node;
            node = nullptr;
        }

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
    }
    default: {
        [[unlikely]];
        break;
    }
    }

    return oldNode;
}

} // namespace Platform
