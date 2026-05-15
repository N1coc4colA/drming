#include "videoframeitem.h"

#include <QSGSimpleTextureNode>

QRectF fitKeepAspect(const QSizeF& itemSize, const QSizeF& imgSize)
{
    if (itemSize.isEmpty() || imgSize.isEmpty()) {
        return QRectF();
    }

    const qreal sx = itemSize.width() / imgSize.width();
    const qreal sy = itemSize.height() / imgSize.height();
    const qreal s = qMin(sx, sy);

    const qreal w = imgSize.width() * s;
    const qreal h = imgSize.height() * s;

    const qreal x = (itemSize.width() - w) * 0.5;
    const qreal y = (itemSize.height() - h) * 0.5;

    return QRectF(x, y, w, h);
}

VideoFrameItem::VideoFrameItem(QQuickItem* parent)
    : QQuickItem(parent)
{
    setFlag(ItemHasContents, true);
}

inline void submit(QQuickWindow* window, const QRectF& dst, const QImage& localImage, QSGSimpleTextureNode*& node)
{
    if (!node) {
        node = new QSGSimpleTextureNode();
        node->setFiltering(QSGTexture::Linear);
    }

    node->setTexture(window->createTextureFromImage(localImage, QQuickWindow::TextureIsOpaque));
    node->setOwnsTexture(true);
    node->setRect(dst);
    node->markDirty(QSGNode::DirtyMaterial | QSGNode::DirtyGeometry);
}

QSGNode* VideoFrameItem::updatePaintNode(QSGNode* oldNode, UpdatePaintNodeData* data)
{
    Q_UNUSED(data);

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

    if (localImage.isNull()) {
        return nullptr;
    }

    auto* node = static_cast<QSGSimpleTextureNode*>(oldNode);

    if (dirty) {
        const QRectF dst = fitKeepAspect(QSizeF(width(), height()), QSizeF(localImage.size()));
        if (!node) {
            node = new QSGSimpleTextureNode();
            node->setFiltering(QSGTexture::Linear);
        }

        node->setTexture(window()->createTextureFromImage(localImage, QQuickWindow::TextureIsOpaque));
        node->setOwnsTexture(true);
        node->setRect(dst);
        node->markDirty(QSGNode::DirtyMaterial | QSGNode::DirtyGeometry);
    }

    return node;
}
