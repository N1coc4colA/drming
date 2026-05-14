#include "videoframeitem.h"

#include <QSGSimpleTextureNode>

QRectF fitKeepAspect(const QSizeF& itemSize, const QSizeF& imgSize)
{
    if (itemSize.isEmpty() || imgSize.isEmpty())
        return QRectF();

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

QSGNode* VideoFrameItem::updatePaintNode(QSGNode* oldNode, UpdatePaintNodeData* data)
{
    Q_UNUSED(data);
    auto* node = static_cast<QSGSimpleTextureNode*>(oldNode);

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

    if (!dirty && node) {
        return node;
    }

    if (localImage.isNull()) {
        delete node;
        return nullptr;
    }

    if (!node) {
        node = new QSGSimpleTextureNode();
        node->setFiltering(QSGTexture::Linear);
    }

    const QRectF dst = fitKeepAspect(QSizeF(width(), height()), QSizeF(localImage.size()));

    // createTextureFromImage() is safe here because we ARE on the render thread
    QSGTexture* newTex = window()->createTextureFromImage(localImage, QQuickWindow::TextureIsOpaque);

    // setOwnsTexture(true) means the node destroys the OLD texture automatically
    // when you call setTexture() with a new one — but ONLY if it already owns one.
    // Set it before replacing, so the old texture is freed correctly.
    node->setOwnsTexture(true);
    node->setTexture(newTex);
    node->setRect(dst);
    node->markDirty(QSGNode::DirtyMaterial | QSGNode::DirtyGeometry);

    return node;
}
