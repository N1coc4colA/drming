#include "networklink.h"
#include "ffmpeg.h"
#include "videoframeitem.h"

namespace Platform {

NetworkLink::NetworkLink(QObject *parent)
    : ::NetworkLink(parent)
    , Parser(*this)
{
    QObject::connect(this, &NetworkLink::connectionInitialised, this, [this]() { Parser::clear(); });
    QObject::connect(this, &NetworkLink::closed, this, [this]() {
        m_locked = false;
        m_waitedForResolution = false;
        Parser::clearRules();
    });
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
    Parser::clear();
}

void NetworkLink::processPacket(const Packets::Reinit &)
{
    Parser::clear();
}

void NetworkLink::processPacket(const Packets::ClientResolution &res)
{
    setBounds<&Packets::ServerImage::data>(quint64(0), static_cast<quint64>(res.width.data) * static_cast<quint64>(res.height.data));
}

void NetworkLink::processPacket(const Packets::ServerStream &img)
{
    if (!m_item || !m_decoder) [[unlikely]] {
        return;
    }

    if (!m_locked) [[unlikely]] {
        disable(Packets::Type::ServerImage);
        m_locked = true;
    }

    m_decoder->decode(reinterpret_cast<const uint8_t *>(img.data.data.constData()), img.data.size);
    m_item->update();
}

void NetworkLink::processPacket(const Packets::ServerImage &img)
{
    if (!m_item) [[unlikely]] {
        return;
    }

    if (!m_locked) [[unlikely]] {
        disable(Packets::Type::ServerStream);
        m_locked = true;
    }

    const QImage converted = QImage::fromData(img.data.data, img.format.data);
    if (!converted.isNull()) [[unlikely]] {
        // Will trigger repaint.
        m_item->setImage(converted);
    }
}

} // namespace Platform
