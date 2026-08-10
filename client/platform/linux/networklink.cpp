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
}

void NetworkLink::onPacketErrors()
{
    Parser::clear();
}

void NetworkLink::processPacket(const Packets::Reinit &)
{
    Parser::clear();
}

void NetworkLink::processPacket(const Packets::ServerStream &img)
{
    if (!m_item) {
        return;
    }

    if (!m_locked) [[unlikely]] {
        disable(Packets::Type::ServerImage);
        m_locked = true;
    }

    if (!m_decoder) {
        m_decoder = new FfmpegDecoder(this);
        m_decoder->setFrameCallback([this](const QImage &image) {
            if (!image.isNull()) {
                [[likely]];

                m_item->setImage(image);
            }
        });
    }

    m_decoder->decode(reinterpret_cast<const uint8_t *>(img.data.data.constData()), img.data.size);
}

void NetworkLink::processPacket(const Packets::ServerImage &img)
{
    if (!m_item) {
        return;
    }

    if (!m_locked) [[unlikely]] {
        disable(Packets::Type::ServerStream);
        m_locked = true;
    }

    const auto converted = QImage::fromData(img.data.data, img.format.data);

    if (!converted.isNull()) [[likely]] {
        m_item->setImage(converted);
    }
}

} // namespace Platform
