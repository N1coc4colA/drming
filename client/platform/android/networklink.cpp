#include "networklink.h"
#include "ffmpeg.h"
#include "videoframeitem.h"

namespace Platform {

NetworkLink::NetworkLink(QObject *parent)
    : ::NetworkLink(parent)
    , Parser(*this)
{
    QObject::connect(this, &NetworkLink::connectionInitialised, this, [this]() { Parser::clear(); });
}

void NetworkLink::setItem(QObject *item)
{
    m_item = qobject_cast<VideoFrameItem *>(item);
    if (!m_item) {
        qWarning() << "NetworkLink::setItem expects a VideoFrameItem object.";
        return;
    }

    // Create decoder if needed
    if (!m_decoder) {
        m_decoder = new FfmpegDecoder(this);
    }

    // Tell the item to use the decoder's OES texture
    m_item->setDecoder(m_decoder);
}

void NetworkLink::onPacketErrors()
{
    qDebug() << "Clearing, too many errors";

    Parser::clear();

    const Packets::Reinit reinit{};
    write(Packets::Writer::generate(reinit));
}

void NetworkLink::processPacket(const Packets::Reinit &)
{
    qDebug() << "Reiniting";
    Parser::clear();
}

void NetworkLink::processPacket(const Packets::ServerStream &img)
{
    if (!m_item || !m_decoder) [[unlikely]] {
        return;
    }

    m_decoder->decode(reinterpret_cast<const uint8_t *>(img.data.constData()), img.frameSize);
    m_item->update();
}

void NetworkLink::processPacket(const Packets::ServerImage &img)
{
    qDebug() << "IMG";

    if (!m_item) [[unlikely]] {
        return;
    }

    const QImage converted = QImage::fromData(img.data, img.format);
    if (!converted.isNull()) [[unlikely]] {
        // Will trigger repaint.
        m_item->setImage(converted);
    }
}

} // namespace Platform
