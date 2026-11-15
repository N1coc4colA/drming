#include "videoframeitem.h"

#include <QSGSimpleTextureNode>
#include <QThread>

#include <ctrack.hpp>

#include "androidui.h"
#include "decode.h"
#include "oesrendernode.h"

#include "../net/telemetry.h"

namespace Platform {

/// Computes a centered rectangle that preserves image aspect ratio.
static QRectF fitKeepAspect(const QSizeF &itemSize, const QSizeF &imgSize)
{
    CTRACK;

    if (itemSize.isEmpty() || imgSize.isEmpty()) [[unlikely]] {
        return {};
    }

    const auto sx = itemSize.width() / imgSize.width();
    const auto sy = itemSize.height() / imgSize.height();
    const auto s = qMin(sx, sy);
    const auto w = imgSize.width() * s;
    const auto h = imgSize.height() * s;
    return { (itemSize.width() - w) * 0.5, (itemSize.height() - h) * 0.5, w, h };
}

VideoFrameItem::VideoFrameItem(QQuickItem *parent)
    : QQuickItem(parent)
    , m_aui(new AndroidUi(this))
{
    setFlag(ItemHasContents, true);

    connect(this, &VideoFrameItem::visibleChanged, this, [this] {
        isVisible() ? m_aui->enterFullScreen() : m_aui->leaveFullScreen();
    });
    // In VideoFrameItem's ctor, if m_decoder is set:
    connect(window(), &QQuickWindow::sceneGraphInvalidated, this, [this] {
        if (m_decoder) {
            m_decoder->invalidateGLResources();
        }
    });
}

void VideoFrameItem::setDecoder(VideoDecoder *decoder)
{
    m_decoder = decoder;

    const auto texMode = m_textureMode.load(std::memory_order_acquire);
    if (decoder) [[likely]] {
        if (texMode != OES) [[unlikely]] {
            m_previousTextureMode.store(texMode, std::memory_order_release);
            m_textureMode.store(OES, std::memory_order_release);
        }

        update();
    }
}


void VideoFrameItem::setImage(const QImage &image, const quint64 receiveNs)
{
    if (image.isNull()) [[unlikely]] {
        return;
    }

    CTRACK;

    {
        QMutexLocker locker(&m_mtx);
        m_frameQueue.push_back({ .image = image, .receiveNs = receiveNs });
        while (m_frameQueue.size() > MaxPresentationQueue) {
            m_frameQueue.pop_front(); // drop the oldest frame
            if (m_telemetry) {
                m_telemetry->recordDropped();
            }
        }
    }

    const auto texMode = m_textureMode.load(std::memory_order_acquire);
    if (texMode != Simple) [[unlikely]] {
        m_previousTextureMode.store(texMode, std::memory_order_release);
        m_textureMode.store(Simple, std::memory_order_release);
    }

    if (thread() == QThread::currentThread()) {
        update();
    } else {
        QMetaObject::invokeMethod(this, [this] { update(); }, Qt::QueuedConnection);
    }
}


void VideoFrameItem::geometryChange(const QRectF &newGeometry, const QRectF &oldGeometry)
{
    QQuickItem::geometryChange(newGeometry, oldGeometry);

    if (newGeometry.size() != oldGeometry.size()) {
        update();
    }
}


QSGNode *VideoFrameItem::updatePaintNode(QSGNode *oldNode, UpdatePaintNodeData *data)
{
    CTRACK;

    Q_UNUSED(data);

    const auto texMode = m_textureMode.load(std::memory_order_acquire);

    if (texMode != m_previousTextureMode) {
        if (oldNode) {
            delete oldNode;
            oldNode = nullptr;
        }
        m_previousTextureMode.store(texMode, std::memory_order_release);
    }

    switch (texMode) {
    case OES: {
        if (m_decoder) [[likely]] {
            auto node = oldNode;
            if (!node) [[unlikely]] {
                node = new QSGNode();
            }

            if (m_decoder->consumeFrame()) {
                if (const auto texId = m_decoder->oesTextureId(); texId.has_value()) [[unlikely]] {
                    auto oesNode = node->childCount() ? static_cast<OESRenderNode *>(node->firstChild()) : nullptr;
                    if (!oesNode) [[unlikely]] {
                        oesNode = new OESRenderNode();
                        node->removeAllChildNodes();
                        node->appendChildNode(oesNode);
                    }

                    oesNode->setTextureId(texId.value());
                    oesNode->setRect(fitKeepAspect(QSizeF(width(), height()), m_decoder->textureSize()));
                    oesNode->markDirty(QSGNode::DirtyMaterial);
                }
            }

            return node;
        }

        break;
    }
    case Simple: {
        auto node = static_cast<QSGSimpleTextureNode *>(oldNode);
        if (!node) [[unlikely]] {
            node = new QSGSimpleTextureNode();
            node->setFiltering(QSGTexture::Linear);
        }

        bool textureChanged = false;
        QImage localImage{};
        bool hasMore = false;

        {
            QMutexLocker locker(&m_mtx);
            if (!m_frameQueue.empty()) {
                auto [image, receiveNs] = std::move(m_frameQueue.front());
                m_frameQueue.pop_front();
                localImage = std::move(image);
                m_imageSize = localImage.size();
                textureChanged = true;
                hasMore = !m_frameQueue.empty();

                if (m_telemetry) {
                    m_telemetry->setRenderQueueDepth(narrow<quint8>(m_frameQueue.size()));
                }

                if (receiveNs > 0 && m_latencyCallback) {
                    m_latencyCallback(steadyNowNs() - receiveNs);
                }
            }
        }

        if (textureChanged && !localImage.isNull()) [[likely]] {
            if (const auto fallbackTex = window()->createTextureFromImage(localImage, QQuickWindow::TextureIsOpaque))
                [[likely]] {
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
