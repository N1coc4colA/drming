#include "videoframeitem.h"

#include <QOpenGLContext>
#include <QQuickWindow>
#include <QSGSimpleTextureNode>
#include <QThread>

#include "decode.h"
#include "oesrendernode.h"

namespace Platform {

QRectF fitKeepAspect(const QSizeF& itemSize, const QSizeF& imgSize)
{
    if (itemSize.isEmpty() || imgSize.isEmpty()) [[unlikely]] {
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

void VideoFrameItem::setDecoder(VideoDecoder* decoder)
{
    m_decoder = decoder;

    if (decoder) [[unlikely]] {
        if (m_textureMode != TextureMode::OES) [[unlikely]] {
            m_previousTextureMode = m_textureMode;
            m_textureMode = TextureMode::OES;
        }

        update();
    }
}

void VideoFrameItem::setImage(const QImage& image)
{
    if (image.isNull()) [[unlikely]] {
        return;
    }

    {
        QMutexLocker locker(&m_mtx);
        m_frameQueue.push_back(image);
        while (m_frameQueue.size() > MaxPresentationQueue) {
            m_frameQueue.pop_front(); // drop oldest frame
        }
    }

    if (m_textureMode != TextureMode::Simple) [[unlikely]] {
        m_previousTextureMode = m_textureMode;
        m_textureMode = TextureMode::Simple;
    }

    if (thread() == QThread::currentThread()) {
        update();
    } else {
        QMetaObject::invokeMethod(this, [this]() { update(); }, Qt::QueuedConnection);
    }
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

    if (m_textureMode != m_previousTextureMode) {
        if (oldNode) {
            delete oldNode;
            oldNode = nullptr;
        }
        m_previousTextureMode = m_textureMode;
    }

    switch (m_textureMode) {
    case TextureMode::OES: {
        if (m_decoder) [[unlikely]] {
            auto node = static_cast<QSGNode*>(oldNode);
            if (!node) [[unlikely]] {
                node = new QSGNode();
            }

            if (m_decoder->consumeFrame()) {
                const auto texId = m_decoder->oesTextureId();
                if (texId != 0) [[unlikely]] {
                    auto oesNode = node->childCount() ? static_cast<OESRenderNode*>(node->firstChild()) : nullptr;
                    if (!oesNode) [[unlikely]] {
                        oesNode = new OESRenderNode();
                        node->removeAllChildNodes();
                        node->appendChildNode(oesNode);
                    }

                    oesNode->setTextureId(texId);
                    oesNode->setRect(fitKeepAspect(QSizeF(width(), height()), m_decoder->textureSize()));
                    oesNode->markDirty(QSGNode::DirtyMaterial);
                }
            }

            return node;
        }

        break;
    }
    case TextureMode::Simple: {
        auto node = static_cast<QSGSimpleTextureNode*>(oldNode);
        if (!node) [[unlikely]] {
            node = new QSGSimpleTextureNode();
            node->setFiltering(QSGTexture::Linear);
        }

        bool textureChanged = false;
        QImage localImage;
        bool hasMore = false;

        {
            QMutexLocker locker(&m_mtx);
            if (!m_frameQueue.empty()) {
                localImage = m_frameQueue.front();
                m_frameQueue.pop_front();
                m_imageSize = localImage.size();
                textureChanged = true;
                hasMore = !m_frameQueue.empty();
            }
        }

        if (textureChanged && !localImage.isNull()) [[likely]] {
            if (auto fallbackTex = window()->createTextureFromImage(localImage, QQuickWindow::TextureIsOpaque)) [[likely]] {
                fallbackTex->setFiltering(QSGTexture::Linear);
                node->setTexture(fallbackTex);
                node->setOwnsTexture(true);
            }
        }

        // Always update geometry (aspect-fit) even if texture didn't change
        const QRectF rect = fitKeepAspect(size(), m_imageSize);
        node->setRect(rect);

        // Mark dirty if texture or geometry changed
        if (textureChanged || node->rect() != rect) {
            node->markDirty(QSGNode::DirtyMaterial | QSGNode::DirtyGeometry);
        }

        // If another frame is already waiting in the 2-frame buffer, trigger update for next frame
        if (hasMore) {
            update();
        }

        return node;
    }
    default: {
        break;
    }
    }

    return oldNode;
}

} // namespace Platform
