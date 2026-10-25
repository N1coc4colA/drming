#include "display.h"

#include <QBuffer>
#include <QDtls>
#include <QtEndian>

#include <chrono>

#include "../net/parser.h"
#include "../settings.h"

#include "ffmpeg.h"
#include "parameters.h"
#include "streamsocket.h"

#ifndef Q_MOC_RUN
#include <ctrack.hpp>
#endif

Display::Display(const QString &connectorName, StreamSocket &ss, const bool inPlaceComposite, QObject *parent)
    : QObject(parent)
    , m_ss(ss)
    , m_reader(connectorName, inPlaceComposite)
{
    connect(&m_pollTimer, &QTimer::timeout, this, &Display::forward);
    m_pollTimer.setTimerType(Qt::PreciseTimer);
    m_pollTimer.setInterval(Settings::pollMSecsInterval);
    m_pollTimer.stop();
}

void Display::applyClientTelemetry(const Packets::ClientTelemetry &t)
{
    const int current = m_targetFps.load(std::memory_order_relaxed);
    int next = current;

    // Overload is sustained. A drop or a 60 ms p95 is normal jitter at 10-40 fps.
    // 3 consecutive samples (6 s) shows a capacity issue.
    const bool overloaded = t.droppedFrames.data >= 3 || t.latencyMsP95.data > 60 || t.decodeQueueDepth.data > 4
        || t.renderQueueDepth.data > 4;

    const bool healthy = t.droppedFrames.data == 0 && t.latencyMsP95.data < 30 && t.decodeQueueDepth.data < 2
        && t.renderQueueDepth.data < 2;

    if (overloaded) {
        if (++m_overloadStreak >= 3) {
            next = std::max(10, current - 5);
            m_overloadStreak = 0;
        }
        m_healthyStreak = 0;
    } else if (healthy) {
        if (++m_healthyStreak >= 3) {
            next = std::min(Settings::maxForwardFps, current + 5);
            m_healthyStreak = 0;
        }
        m_overloadStreak = 0;
    } else {
        // Mixed signals: reset both streak counters and leave target alone.
        m_overloadStreak = 0;
        m_healthyStreak = 0;
    }

    if (next != current) {
        qInfo().nospace() << "Telemetry: fps=" << (t.fpsX10.data / 10.0) << " p95=" << t.latencyMsP95.data << "ms"
                          << " drops=" << t.droppedFrames.data << " target " << current << "→" << next;
        m_targetFps.store(next, std::memory_order_relaxed);
    }
}

void Display::addClient(NetworkClient *client)
{
    if (!client) [[unlikely]] {
        return;
    }

    qDebug() << "Adding client to display (A)";

    assert(!m_clients.contains(client));

    qDebug() << "Adding client to display (B)";

    client->setParent(this);

    m_clients.insert(client);
    connect(client, &NetworkClient::disconnected, this, &Display::onDisconnected);

    qDebug() << "Client connected" << client->peerAddress() << client->peerPort();

    m_targetFps.store(Settings::maxForwardFps, std::memory_order_relaxed);

    client->setDisplay(this);
    onConnected();
}

void Display::reinit()
{
    m_lastFbId = 0;
    if (m_pollTimer.isActive()) {
        m_pollTimer.stop();
        QTimer::singleShot(1500, [this]() {
            if (!m_clients.isEmpty()) {
                const Packets::Reinit reinitPkt{};
                sendData(Packets::Writer::generate(reinitPkt));
                m_pollTimer.start();
                m_prevSize = {};
                qDebug() << "Restarted screen poll";
            }
        });
    }
}

void Display::requireResolutionInformation()
{
    m_lastFbId = 0;
    m_prevSize = {};
}

void Display::forward()
{
    CTRACK;

    const auto nowNs = std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now().time_since_epoch()
    )
                           .count();

    const auto targetFps = std::max(1, m_targetFps.load(std::memory_order_relaxed));
    const auto minIntervalNs = 1'000'000'000LL / targetFps;

    if (m_lastForwardNs != 0 && (nowNs - m_lastForwardNs) < minIntervalNs) {
        return;
    }

    VkmsFrameBuffer fb{};
    if (!m_reader.getVkmsFrameBuffer(fb)) [[unlikely]] {
        if (!primaryFailureNotice) {
            primaryFailureNotice = true;
            qCritical() << "Failed to get primary";
        }
        return;
    }
    primaryFailureNotice = false;

    // ---- change detection --------------------------------------------
    // VKMS allocates a new framebuffer on every compositor flip, so a
    // stable buffer_id means a stable image. Skip the rest of forward()
    // entirely; on 46 µs getCrtc + ~1 µs compare, this is ~0.9 % CPU
    // at 200 Hz polling.
    if (fb.fb_id == m_lastFbId) [[likely]] {
        DisplayReader::releaseVkmsFrameBuffer(fb);
        return;
    }
    m_lastFbId = fb.fb_id;

    // ---- existing body (composite, encode, send) ---------------------
    CursorFrameBuffer cursorFb{};
    const auto hasCursor = m_reader.getCursorFrameBuffer(cursorFb, fb);

    if (!m_vkmsFrameDescriptor.has_value()) [[unlikely]] {
        m_vkmsFrameDescriptor = DrmFormat::resolve(fb.format);
        if (m_vkmsFrameDescriptor->qtFormat == QImage::Format_Invalid) [[unlikely]] {
            char a, b, c, d;
            Drm::split_fourcc(fb.format, a, b, c, d);
            qWarning() << "Image format for frame is invalid:" << fb.format << ";" << a << b << c << d;
            if (hasCursor)
                DisplayReader::releaseCursorFrameBuffer(cursorFb);
            disconnectAllClients();
            return;
        }
    }
    if (m_vkmsFrameDescriptor->qtFormat == QImage::Format_Invalid) [[unlikely]] {
        if (hasCursor) {
            DisplayReader::releaseCursorFrameBuffer(cursorFb);
        }

        return;
    }

    const bool useInPlace = m_reader.supportsInPlaceComposite() && m_vkmsFrameDescriptor->convert == nullptr;

    auto result = DisplayReader::imageFromFrameBuffer(
        static_cast<const uint8_t *>(fb.data),
        fb.width,
        fb.height,
        fb.stride,
        m_vkmsFrameDescriptor.value(),
        useInPlace
    );

    const QImage *finalImage = &result;

    if (hasCursor) [[unlikely]] {
        if (!m_cursorFrameDescriptor.has_value()) [[unlikely]] {
            m_cursorFrameDescriptor = DrmFormat::resolve(cursorFb.format);
            if (m_cursorFrameDescriptor->qtFormat == QImage::Format_Invalid) [[unlikely]] {
                char a, b, c, d;
                Drm::split_fourcc(cursorFb.format, a, b, c, d);
                qWarning() << "Image format for cursor is invalid:" << cursorFb.format << ";" << a << b << c << d;
                DisplayReader::releaseCursorFrameBuffer(cursorFb);
                disconnectAllClients();
                return;
            }
        }
        if (m_cursorFrameDescriptor->qtFormat == QImage::Format_Invalid) [[unlikely]] {
            DisplayReader::releaseCursorFrameBuffer(cursorFb);
            return;
        }

        if (useInPlace) {
            DisplayReader::compositeWithCursorInPlace(result, cursorFb, m_cursorFrameDescriptor.value());
        } else {
            DisplayReader::compositeWithCursor(result, cursorFb, m_cursorFrameDescriptor.value(), m_compositeBuffer);
            finalImage = &m_compositeBuffer;
        }

        DisplayReader::releaseCursorFrameBuffer(cursorFb);
    }

    if (m_prevSize != finalImage->size()) [[unlikely]] {
        m_prevSize = finalImage->size();
        const Packets::ClientResolution res{
            .width = { static_cast<quint32>(finalImage->width()) },
            .height = { static_cast<quint32>(finalImage->height()) },
        };

        sendData(std::move(Packets::Writer::generate(res)));
    }

    const auto encodeStart = std::chrono::steady_clock::now();

    processImage(*finalImage);

    const auto encodeEnd = std::chrono::steady_clock::now();
    m_encodeSumNs += std::chrono::duration_cast<std::chrono::nanoseconds>(encodeEnd - encodeStart).count();
    if (++m_encodeSamples >= 50) {
#ifdef TELEMETRY_OUTPUT
        qInfo().nospace() << "Encode avg: " << (m_encodeSumNs / m_encodeSamples / 1'000'000) << " ms"
                          << " (frames=" << m_encodeSamples << ", targetFps=" << targetFps
                          << ", encode time budget=" << (minIntervalNs / 1'000'000) << " ms)";
#endif

        m_encodeSumNs = 0;
        m_encodeSamples = 0;
    }

    DisplayReader::releaseVkmsFrameBuffer(fb);

    m_lastForwardNs = nowNs;
}

void Display::onConnected()
{
    // New clients need to see the current frame even if the compositor hasn't drawn
    // since the previous client was with us.
    m_lastFbId = 0;
    if (!m_pollTimer.isActive()) {
        m_pollTimer.start();
    }
}

void Display::onDisconnected()
{
    if (auto client = qobject_cast<NetworkClient *>(sender())) {
        qDebug() << "Client disconnected" << client->peerAddress() << client->peerPort();
        m_clients.remove(client);
        client->deleteLater();
    }

    if (m_clients.isEmpty()) {
        qDebug() << "No clients anymore, suspending screen.";
        m_pollTimer.stop();
        Q_EMIT nowFree();
    }

    m_targetFps.store(Settings::maxForwardFps, std::memory_order_relaxed);
}

bool Display::sendData(QByteArray output)
{
    return m_ss.write(output);
}

void Display::disconnectAllClients()
{
    if (!m_clients.isEmpty()) {
        for (const auto &client : m_clients) {
            client->close();
        }
    }
}

class DisplayImage : public Display
{
public:
    DisplayImage(
        const QString &connectorName,
        StreamSocket &ss,
        const QString &format,
        const bool inPlaceComposite,
        QObject *parent = nullptr
    )
        : Display(connectorName, ss, inPlaceComposite, parent)
        , m_format(format.toLocal8Bit())
    {}

private:
    const QByteArray m_format;
    std::chrono::time_point<std::chrono::high_resolution_clock> m_last = std::chrono::high_resolution_clock::now();

    void processImage(const QImage &result) override
    {
        CTRACK;

        Packets::ServerImage servImg{};
        servImg.format.data = m_format;
        {
            QBuffer buf(&servImg.data.data);
            buf.open(QIODevice::WriteOnly);
            result.save(&buf, m_format, Parameters::instance.qualityLevel);
            buf.close();
        }

        const auto curr = std::chrono::high_resolution_clock::now();
        const auto duration = duration_cast<std::chrono::microseconds>(curr - m_last);
        // [CHRONO]
        qDebug() << duration;
        m_last = curr;

        sendData(std::move(Packets::Writer::generate(servImg)));
    }
};

class DisplayH265 : public Display
{
public:
    DisplayH265(const QString &connectorName, StreamSocket &ss, const bool inPlaceComposite, QObject *parent = nullptr)
        : Display(connectorName, ss, inPlaceComposite, parent)
        , m_encoder(
              [this](const uint8_t *data, size_t size, int64_t pts) { this->h265dataForward(data, size, pts); },
              Settings::maxForwardFps
          )
    {}

    void forceKeyframe() override
    {
        m_encoder.forceKeyframe();
    }

private:
    void processImage(const QImage &result) override
    {
        CTRACK;

        m_encoder.push_image(result.bits(), result.width(), result.height(), result.format(), result.bytesPerLine());
    }

    void h265dataForward(const uint8_t *data, const size_t size, const int64_t pts)
    {
        CTRACK;

        Q_UNUSED(pts);

        QByteArray payload;
        payload.reserve(static_cast<qsizetype>(size) + 4);
        payload.append("\x00\x00\x00\x01", 4);
        payload.append(reinterpret_cast<const char *>(data), static_cast<qsizetype>(size));

        Packets::ServerStream stm{ .data = { std::move(payload) } };

        if (!sendData(std::move(Packets::Writer::generate(stm)))) {
            /*
             * Frame was truncated in transit (EAGAIN from sendmmsg). The client
             * cannot complete this timestamp in its jitter buffer and will hold
             * the last good reference, producing motion-artifacts until the next
             * natural IDR. Force an IDR on the next encode so recovery is one
             * frame instead of one GOP.
             */
            m_encoder.forceKeyframe();
        }
    }

    Ffmpeg::Encoder m_encoder;
};

Display *generateNewDisplay(const QString &connectorName, StreamSocket &ss, QObject *parent)
{
    constexpr bool inPlaceComposite = false;

    switch (Parameters::instance.streamFormat) {
    case Opts::DisplayStreamType::h265: {
        return new DisplayH265(connectorName, ss, inPlaceComposite, parent);
    }
    case Opts::DisplayStreamType::jpg: {
        return new DisplayImage(connectorName, ss, "JPG", inPlaceComposite, parent);
    }
    case Opts::DisplayStreamType::png: {
        return new DisplayImage(connectorName, ss, "PNG", inPlaceComposite, parent);
    }
    case Opts::DisplayStreamType::webp: {
        return new DisplayImage(connectorName, ss, "WEBP", inPlaceComposite, parent);
    }
    }

    // [TODO] Generate an error message.
    return nullptr;
}
