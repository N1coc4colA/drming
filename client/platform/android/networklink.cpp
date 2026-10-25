#include "networklink.h"
#include "decode.h"
#include "videoframeitem.h"

namespace Platform {

VideoLink::VideoLink(QObject *parent)
    : UdpStreamSocket(parent)
    , Packets::Parser<VideoLink, Settings::errorLimit>(*this)
{
    QObject::connect(this, &UdpStreamSocket::connected, []() { qDebug() << "VideoLink connected."; });
}

void VideoLink::processPacket(const Packets::ClientResolution &res)
{
    m_waitedForResolution.store(true, std::memory_order_relaxed);
    setBounds<&Packets::ServerImage::data>(
        quint64(0),
        static_cast<quint64>(res.width.data) * static_cast<quint64>(res.height.data) * 4
    );
}

void VideoLink::processPacket(const Packets::ServerStream &img)
{
    if (!m_item || !m_decoder) [[unlikely]] {
        return;
    }

    if (!m_locked) [[unlikely]] {
        disable(Packets::Type::ServerImage);
        m_locked = true;
    }

    const auto recvNs = steadyNowNs();
    if (m_telemetry) {
        m_telemetry->recordReceived(img.data.size);
    }
    m_decoder->setLastInputNs(recvNs);

    m_decoder->decode(reinterpret_cast<const uint8_t *>(img.data.data.constData()), img.data.size);
    QMetaObject::invokeMethod(m_item, "update", Qt::QueuedConnection);
}

void VideoLink::processPacket(const Packets::ServerImage &img)
{
    if (!m_item) [[unlikely]] {
        return;
    }

    if (!m_locked) [[unlikely]] {
        disable(Packets::Type::ServerStream);
        m_locked = true;
    }

    const auto recvNs = steadyNowNs();
    if (m_telemetry) {
        m_telemetry->recordReceived(img.data.data.size());
    }

    // Offload decompression and format conversion to background decoder thread with frame dropping
    m_imageDecoder.queueFrame(img.data.data, img.format.data, recvNs);
}

void VideoLink::setTelemetry(Telemetry *t)
{
    m_telemetry = t;
    m_imageDecoder.setTelemetry(t);

    if (m_decoder) {
        m_decoder->setTelemetry(t);
        m_decoder->setLatencyCallback([t](const qint64 ns) {
            if (t) {
                t->recordDecoded(ns);
            }
        });
    }
}

void VideoLink::onFrameSkipped()
{
    qDebug() << "JitterBuffer: skipped incomplete frame — requesting IDR";
    if (m_telemetry) {
        m_telemetry->recordDropped();
    }

    Q_EMIT keyframeRequested();
}

void VideoLink::setItem(VideoFrameItem *item)
{
    m_item = item;

    // Create hardware decoder if needed for H.265/video streams
    if (!m_decoder) {
        qDebug() << "Creating video decoder";
        m_decoder = new VideoDecoder(this);
        if (m_telemetry) {
            m_decoder->setTelemetry(m_telemetry);
            m_decoder->setLatencyCallback([t = m_telemetry](qint64 ns) {
                if (t)
                    t->recordDecoded(ns);
            });
        }

        QObject::connect(
            m_decoder,
            &VideoDecoder::needsKeyframe,
            this,
            &VideoLink::keyframeRequested,
            Qt::DirectConnection
        );
    }

    // Connect image decoder callback to update UI safely on GUI thread
    m_imageDecoder.setFrameCallback([this](const QImage &image, qint64 receiveNs) {
        if (m_item) {
            QMetaObject::invokeMethod(
                m_item,
                "setImage",
                Qt::QueuedConnection,
                Q_ARG(QImage, image),
                Q_ARG(qint64, receiveNs)
            );
        }
    });

    // Hop to the item's thread for the mutations.
    QMetaObject::invokeMethod(
        item,
        [this, item] {
            if (m_telemetry) {
                item->setLatencyCallback([t = m_telemetry](qint64 ns) {
                    if (t)
                        t->recordDecoded(ns);
                });
            }
            item->setDecoder(m_decoder);
        },
        Qt::QueuedConnection
    );
}

NetworkLink::NetworkLink(QObject *parent)
    : ::NetworkLink(parent)
    , Parser(*this)
{
    m_vlink = new VideoLink();
    m_alink = new AudioLink();

    m_vlink->moveToThread(&m_videoThread);
    m_alink->moveToThread(&m_audioThread);

    m_vlink->setTelemetry(&m_telemetry);

    QObject::connect(&m_videoThread, &QThread::finished, m_vlink, &QObject::deleteLater);
    QObject::connect(&m_audioThread, &QThread::finished, m_alink, &QObject::deleteLater);

    m_videoThread.setObjectName("VideoLinkThread");
    m_audioThread.setObjectName("AudioLinkThread");

    m_videoThread.start();
    m_audioThread.start();

    QObject::connect(m_alink, &UdpStreamSocket::disconnected, &m_lock, &MulticastLock::release);
    QObject::connect(m_vlink, &UdpStreamSocket::disconnected, &m_lock, &MulticastLock::release);
    QObject::connect(
        m_vlink,
        &VideoLink::keyframeRequested,
        this,
        [this]() {
            static constexpr Packets::RequestKeyframe pkt{};
            write(Packets::Writer::generate(pkt));
        },
        Qt::QueuedConnection
    );

    QObject::connect(this, &NetworkLink::connectionInitialised, this, [this]() {
        QMetaObject::invokeMethod(m_vlink, &VideoLink::reset);
        m_waitedForKey = false;
        Parser::clearRules();
    });
    QObject::connect(this, &NetworkLink::connectionReady, this, [this]() { m_requireCheck.start(); });
    QObject::connect(this, &NetworkLink::closed, this, [this]() {
        QMetaObject::invokeMethod(m_vlink, &VideoLink::reset);
        m_waitedForKey = false;
        Parser::clearRules();
    });
    QObject::connect(&m_requireCheck, &QTimer::timeout, this, &NetworkLink::performRequirements);
    QObject::connect(this, &NetworkLink::closed, &m_requireCheck, &QTimer::stop);

    m_requireCheck.setInterval(500);
    m_requireCheck.stop();
}

NetworkLink::~NetworkLink()
{
    m_requireCheck.stop();

    if (m_vlink) {
        QMetaObject::invokeMethod(m_vlink, &VideoLink::close, Qt::BlockingQueuedConnection);
    }
    if (m_alink) {
        QMetaObject::invokeMethod(m_alink, &AudioLink::close, Qt::BlockingQueuedConnection);
    }

    m_videoThread.quit();
    m_videoThread.wait();

    m_audioThread.quit();
    m_audioThread.wait();
}

void NetworkLink::performRequirements()
{
    const auto ok = m_vlink && m_vlink->waitedForResolution() && m_waitedForKey;

    if (ok) {
        m_requireCheck.stop();
        return;
    }

    if (m_vlink && !m_vlink->waitedForResolution()) [[unlikely]] {
        // m_vlink->waitFor(Packets::Type::ClientResolution);

        static constexpr Packets::RequestClientResolution resReq{};
        write(Packets::Writer::generate(resReq));
    }

    if (!m_waitedForKey) [[unlikely]] {
        waitFor(Packets::Type::KeyUpdate);

        static constexpr Packets::RequestKey keyReq{};
        write(Packets::Writer::generate(keyReq));
    }

    if (!m_waitedForAudioSource) [[unlikely]] {
        waitFor(Packets::Type::AudioSource);

        static constexpr Packets::RequestAudioSource audReq{};
        write(Packets::Writer::generate(audReq));
    }

    if (!m_waitedForVideoSource) [[unlikely]] {
        waitFor(Packets::Type::VideoSource);

        static constexpr Packets::RequestVideoSource vidReq{};
        write(Packets::Writer::generate(vidReq));
    }
}

void NetworkLink::setItem(QObject *item)
{
    auto vitem = qobject_cast<VideoFrameItem *>(item);
    if (!vitem) {
        qWarning() << "NetworkLink::setItem expects a VideoFrameItem object.";
        return;
    }

    QMetaObject::invokeMethod(m_vlink, [this, vitem]() { m_vlink->setItem(vitem); });
}

void NetworkLink::processPacket(const Packets::KeyUpdate &ku)
{
    qDebug() << "Got key" << ku.keyStamp.data << ku.key.data;

    m_inactivityTimer.stop();
    m_waitedForKey = true;

    const auto keyData = ku.key.data;
    const auto keyStamp = ku.keyStamp.data;
    QMetaObject::invokeMethod(m_vlink, [this, keyData, keyStamp]() { m_vlink->setKey(keyData, keyStamp); });
}

void NetworkLink::processPacket(const Packets::AudioSource &src)
{
    qDebug() << "Got audio source:" << QHostAddress(*reinterpret_cast<const quint32 *>(src.ip.data.constData())) << ':'
             << src.port.data;

    m_inactivityTimer.stop();
    const auto addr = src.isV4.data ? QHostAddress(*reinterpret_cast<const quint32 *>(src.ip.data.constData()))
                                    : QHostAddress(*reinterpret_cast<const quint8 *>(src.ip.data.constData()));
    const quint16 port = src.port.data;

    m_waitedForAudioSource = true;
    m_lock.lock();

    QMetaObject::invokeMethod(m_alink, [this, addr, port]() {
        const auto sock = m_alink->socket();
        if (sock ? (sock->peerPort() != port || sock->peerAddress() != addr) : true) {
            m_alink->close();
            m_alink->connectToMulticast(addr, port);
        }
    });
}

void NetworkLink::processPacket(const Packets::VideoSource &src)
{
    qDebug() << "Got video source:" << QHostAddress(*reinterpret_cast<const quint32 *>(src.ip.data.constData())) << ':'
             << src.port.data;

    m_inactivityTimer.stop();
    const auto addr = src.isV4.data ? QHostAddress(*reinterpret_cast<const quint32 *>(src.ip.data.constData()))
                                    : QHostAddress(*reinterpret_cast<const quint8 *>(src.ip.data.constData()));
    const quint16 port = src.port.data;

    m_waitedForVideoSource = true;
    m_lock.lock();

    QMetaObject::invokeMethod(m_vlink, [this, addr, port]() {
        const auto sock = m_vlink->socket();
        if (sock ? (sock->peerPort() != port || sock->peerAddress() != addr) : true) {
            m_vlink->close();
            m_vlink->connectToMulticast(addr, port);
        }
    });
}

} // namespace Platform
