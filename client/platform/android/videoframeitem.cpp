#include "videoframeitem.h"
#include "ffmpeg.h"

#include <QQuickWindow>
#include <QSGSimpleTextureNode>

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

void VideoFrameItem::setDecoder(FfmpegDecoder* decoder)
{
    m_decoder = decoder;
    update();
}

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

    qDebug() << "Node paint UPD.";

#ifdef USE_NAT_SURFACES
    QSGSimpleTextureNode* node = static_cast<QSGSimpleTextureNode*>(oldNode);
    if (!node) {
        node = new QSGSimpleTextureNode();
        node->setFiltering(QSGTexture::Linear);

        if (QSGTexture* fallback = window()->createTextureFromImage({})) {
            node->setTexture(fallback);
            node->setOwnsTexture(true);
        }
    }

    bool textureChanged = false;
    QSize currentSize = m_imageSize;

    // Pull the latest frame (updates the OES texture if available)
    const bool hadNewFrame = m_decoder->consumeFrame();
    const bool usesOES = m_decoder && m_decoder->oesTextureId() != 0;

    // ----- Zero-copy OES path -----
    if (usesOES) {
        qDebug() << "hadNewFrame:" << hadNewFrame;

        const GLuint texId = m_decoder->oesTextureId();
        if (texId != 0) {
            if (QSGTexture* newTexture = QNativeInterface::QSGOpenGLTexture::fromNativeExternalOES(texId,
                                                                                                   window(),
                                                                                                   m_decoder->textureSize(),
                                                                                                   QQuickWindow::TextureIsOpaque)) {
                newTexture->setFiltering(QSGTexture::Linear);

                // Replace only if successful
                if (node->texture() != newTexture) {
                    //delete node->texture();
                    node->setTexture(newTexture);
                    node->setOwnsTexture(true);
                    textureChanged = true;
                }

                // OES textures are usually upside-down
                node->setTextureCoordinatesTransform(QSGSimpleTextureNode::MirrorVertically);
                currentSize = m_decoder->textureSize();
            } else {
                qWarning() << "fromNativeExternalOES failed to create texture for ID" << texId;
                // Keep the old texture – don't change anything
            }
        } else {
            qWarning() << "Invalid OES texture ID (0) from decoder";
        }
    } else {
        qDebug() << "States:" << usesOES << m_decoder;
    }

    // ----- Fallback QImage path -----
    if (!usesOES) {
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
            if (QSGTexture* newTexture = window()->createTextureFromImage(localImage, QQuickWindow::TextureIsOpaque)) {
                newTexture->setFiltering(QSGTexture::Linear);
                node->setTexture(newTexture);
                node->setOwnsTexture(true);
                textureChanged = true;
                currentSize = localImage.size();
            }
        }
    }

    // Update geometry (aspect-fit) only if size changed
    node->setRect(fitKeepAspect(QSizeF(width(), height()), currentSize.isValid() ? currentSize : QSize(1, 1)));

    // Update m_imageSize so geometryChange can detect changes
    m_imageSize = currentSize;

    // Mark dirty if texture or geometry changed
    node->markDirty(textureChanged ? QSGNode::DirtyMaterial : QSGNode::DirtyGeometry);

#else
    QImage localImage{};
    bool dirty = false;
    {
        QMutexLocker locker(&m_mtx);
        if (m_imageDirty) {
            localImage = m_currentImage;
            m_imageDirty = false;
            dirty = true;
        }
    }

    auto node = dynamic_cast<QSGSimpleTextureNode*>(oldNode);
    const QSizeF imgSize(m_imageSize);
    const auto dst = fitKeepAspect(QSizeF(width(), height()), imgSize);

    if (!node && !dirty) {
        return nullptr;
    }

    if (dirty && localImage.isNull()) {
        return nullptr;
    }

    if (!node) {
        node = new QSGSimpleTextureNode();
        node->setFiltering(QSGTexture::Linear);
    }

    if (dirty) {
        node->setTexture(window()->createTextureFromImage(localImage, QQuickWindow::TextureIsOpaque));
        node->setOwnsTexture(true);
    }

    node->setRect(dst);
    node->markDirty(dirty ? QSGNode::DirtyMaterial | QSGNode::DirtyGeometry : QSGNode::DirtyGeometry);
#endif

    return node;
}

} // namespace Platform
