#include "networklink.h"

#include "ffmpeg.h"
#include "videoframeitem.h"

namespace Platform {

NetworkLink::NetworkLink(QObject *parent)
    : ::NetworkLink(parent)
    , Parser(*this)
{}

void NetworkLink::setItem(QObject *item)
{
    m_item = qobject_cast<VideoFrameItem *>(item);

    if (!m_item) {
        qWarning() << "NetworkLink::setItem expects a VideoFrameItem object.";
        return;
    }
}

void NetworkLink::processPacket(const Packets::ServerStream &img)
{
    if (!m_item) {
        return;
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

    m_decoder->decode(reinterpret_cast<const uint8_t *>(img.data.constData()), img.frameSize);
}

void NetworkLink::processPacket(const Packets::ServerImage &img)
{
    if (!m_item) {
        return;
    }

    const auto converted = QImage::fromData(img.data, img.format);

    if (!converted.isNull() && m_item) {
        [[likely]];

        m_item->setImage(converted);
    }
}

void NetworkLink::processPacket(const Packets::ServerBrightness &brightness)
{
    Q_UNUSED(brightness);
}

} // namespace Platform
