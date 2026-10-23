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
    connect(&m_timer, &QTimer::timeout, this, &Display::forward);
    m_timer.setInterval(Settings::frameMSecsInterval);
    m_timer.stop();
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

    client->setDisplay(this);
    onConnected();
}

void Display::reinit()
{
    if (m_timer.isActive()) {
        qDebug() << "Stopping screen timer";

        m_timer.stop();
        QTimer::singleShot(1500, [this]() {
            if (!m_clients.isEmpty()) {
                const Packets::Reinit reinitPkt{};
                sendData(Packets::Writer::generate(reinitPkt));
                m_timer.start();
                qDebug() << "Restarted screen timer";
                m_prevSize = {};
            }
        });
    }
}

void Display::requireResolutionInformation()
{
    m_prevSize = {};
}

void Display::forward()
{
    CTRACK;

    VkmsFrameBuffer fb{};
    if (!m_reader.getVkmsFrameBuffer(fb)) [[unlikely]] {
        if (!primaryFailureNotice) {
            primaryFailureNotice = true;
            qCritical() << "Failed to get primary";
        }

        return;
    }
    primaryFailureNotice = false;

    CursorFrameBuffer cursorFb{};
    const auto hasCursor = m_reader.getCursorFrameBuffer(cursorFb, fb);

    if (!m_vkmsFrameDescriptor.has_value()) [[unlikely]] {
        m_vkmsFrameDescriptor = DrmFormat::resolve(fb.format);
        if (m_vkmsFrameDescriptor->qtFormat == QImage::Format_Invalid) [[unlikely]] {
            char a, b, c, d;
            Drm::split_fourcc(fb.format, a, b, c, d);
            qWarning() << "Image format for frame is invalid:" << fb.format << ";" << a << b << c << d;
            if (hasCursor) {
                DisplayReader::releaseCursorFrameBuffer(cursorFb);
            }

            disconnectAllClients();
            return;
        }
    }
    if (m_vkmsFrameDescriptor->qtFormat == QImage::Format_Invalid) [[unlikely]] {
        if (hasCursor) DisplayReader::releaseCursorFrameBuffer(cursorFb);
        return;
    }

    // A native format (convert == nullptr) — swizzles are one-way and would corrupt the cached mapping if run in-place.
    const bool useInPlace = m_reader.supportsInPlaceComposite() && m_vkmsFrameDescriptor->convert == nullptr;

    QImage result = DisplayReader::imageFromFrameBuffer(
        static_cast<const uint8_t *>(fb.data), fb.width, fb.height, fb.stride,
        m_vkmsFrameDescriptor.value(), useInPlace
    );

    const QImage *finalImage = &result;

    if (hasCursor) [[likely]] {
        if (!m_cursorFrameDescriptor.has_value()) [[unlikely]] {
            m_cursorFrameDescriptor = DrmFormat::resolve(cursorFb.format);
            if (m_cursorFrameDescriptor->qtFormat == QImage::Format_Invalid) [[unlikely]] {
                char a, b, c, d;
                Drm::split_fourcc(cursorFb.format, a, b, c, d);
                qWarning() << "Image format for frame is invalid:" << cursorFb.format << ";" << a << b << c << d;

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
            .width  = {static_cast<quint32>(finalImage->width())},
            .height = {static_cast<quint32>(finalImage->height())},
        };

        sendData(std::move(Packets::Writer::generate(res)));
    }

    processImage(*finalImage);
    DisplayReader::releaseVkmsFrameBuffer(fb);
}

void Display::onConnected()
{
    if (!m_timer.isActive()) {
        m_timer.start();
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

        m_timer.stop();
        Q_EMIT nowFree();
    }
}

void Display::sendData(QByteArray output)
{
    m_ss.write(output);
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
    DisplayImage(const QString &connectorName, StreamSocket &ss, const QString &format, const bool inPlaceComposite, QObject *parent = nullptr)
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
        , m_encoder([this](const uint8_t *data, size_t size, int64_t pts) { this->h265dataForward(data, size, pts); },
                    1000 / Settings::frameMSecsInterval)
    {}

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

        Packets::ServerStream stm{.data = {std::move(payload)}};
        sendData(std::move(Packets::Writer::generate(stm)));
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
