#include "videoframeitem.h"

#include <QSGSimpleTextureNode>
#include <QThread>

namespace Platform {

QRectF fitKeepAspect(const QSizeF &itemSize, const QSizeF &imgSize)
{
    if (itemSize.isEmpty() || imgSize.isEmpty()) {
        return {};
    }

    const auto sx = itemSize.width() / imgSize.width();
    const auto sy = itemSize.height() / imgSize.height();
    const auto s = qMin(sx, sy);

    const auto w = imgSize.width() * s;
    const auto h = imgSize.height() * s;

    const auto x = (itemSize.width() - w) * 0.5;
    const auto y = (itemSize.height() - h) * 0.5;

    return { x, y, w, h };
}

VideoFrameItem::VideoFrameItem(QQuickItem *parent)
    : QQuickItem(parent)
{
    setFlag(ItemHasContents, true);
}

void VideoFrameItem::setImage(const QImage &image)
{
    if (image.isNull()) {
        return;
    }

    {
        QMutexLocker locker(&m_mtx);
        m_frameQueue.push_back(image.convertToFormat(QImage::Format_RGBA8888));
        while (m_frameQueue.size() > MaxPresentationQueue) {
            m_frameQueue.pop_front(); // drop oldest frame
        }
    }

    if (thread() == QThread::currentThread()) {
        update();
    } else {
        QMetaObject::invokeMethod(this, [this]() { update(); }, Qt::QueuedConnection);
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
    Q_UNUSED(data);

    auto node = dynamic_cast<QSGSimpleTextureNode *>(oldNode);

    bool textureChanged = false;
    QImage localImage{};
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

    const QSizeF imgSize(m_imageSize);
    const auto dst = fitKeepAspect(QSizeF(width(), height()), imgSize);

    if (!node && !textureChanged) {
        return nullptr;
    }

    if (textureChanged && localImage.isNull()) {
        return nullptr;
    }

    if (!node) {
        node = new QSGSimpleTextureNode();
        node->setFiltering(QSGTexture::Linear);
    }

    if (textureChanged) {
        node->setTexture(window()->createTextureFromImage(localImage, QQuickWindow::TextureIsOpaque));
        node->setOwnsTexture(true);
    }

    node->setRect(dst);
    node->markDirty(textureChanged ? QSGNode::DirtyMaterial | QSGNode::DirtyGeometry : QSGNode::DirtyGeometry);

    if (hasMore) {
        update();
    }

    return node;
}

} // namespace Platform
