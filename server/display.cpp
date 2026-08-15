#include "display.h"

#include <QBuffer>
#include <QDtls>
#include <QtEndian>

#include "../parser.h"
#include "../settings.h"

#include "ffmpeg.h"
#include "parameters.h"
#include "speaker.h"

Display::Display(const QString &connectorName, QObject *parent)
    : QObject(parent)
    , m_reader(connectorName)
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
    const auto hasCursor = DisplayReader::getCursorFrameBuffer(cursorFb, fb);

    if (!m_vkmsFrameDescriptor.has_value()) [[unlikely]] {
        m_vkmsFrameDescriptor = DrmFormat::resolve(fb.format);

        if (m_vkmsFrameDescriptor->qtFormat == QImage::Format_Invalid) [[unlikely]] {
            char a, b, c, d;
            Drm::split_fourcc(fb.format, a, b, c, d);

            qWarning() << "Image format for frame is invalid:" << fb.format << ";" << a << b << c << d;

            disconnectAllClients();
            return;
        }
    }

    if (m_vkmsFrameDescriptor->qtFormat == QImage::Format_Invalid) [[unlikely]] {
        return;
    }

    auto result = DisplayReader::imageFromFrameBuffer(static_cast<const uint8_t *>(fb.data), fb.width, fb.height, fb.stride, m_vkmsFrameDescriptor.value());
    if (hasCursor) [[likely]] {
        if (!m_cursorFrameDescriptor.has_value()) [[unlikely]] {
            m_cursorFrameDescriptor = DrmFormat::resolve(cursorFb.format);

            if (m_cursorFrameDescriptor->qtFormat == QImage::Format_Invalid) [[unlikely]] {
                char a, b, c, d;
                Drm::split_fourcc(cursorFb.format, a, b, c, d);

                qWarning() << "Image format for frame is invalid:" << cursorFb.format << ";" << a << b << c << d;

                disconnectAllClients();
                return;
            }
        }

        if (m_cursorFrameDescriptor->qtFormat == QImage::Format_Invalid) [[unlikely]] {
            return;
        }

        DisplayReader::compositeWithCursor(result, cursorFb, m_cursorFrameDescriptor.value());
    }

    if (m_prevSize != result.size()) [[unlikely]] {
        m_prevSize = result.size();
        const Packets::ClientResolution res{.width = {static_cast<quint32>(result.width())}, .height = {static_cast<quint32>(result.height())}};
        sendData(std::move(Packets::Writer::generate(res)));
    }

    processImage(result);
    DisplayReader::releaseVkmsFrameBuffer(fb);

    // Send the audio too.
    auto cap = AudioCapture::instance();
    cap->readFrame();

    Packets::ServerAudio audio{};

    {
        const auto &buffer = cap->getBuffer();
        const AudioBufferLock lock(buffer);

        if (!buffer.size()) {
            return;
        }

        audio.data.data = std::move(QByteArray(reinterpret_cast<const char *>(buffer.getData()), static_cast<qsizetype>(buffer.size())));
        audio.frames.data = static_cast<unsigned int>(buffer.frameCount());
    }

    sendData(std::move(Packets::Writer::generate(audio)));
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
    for (const auto &client : m_clients) {
        if (client->state() == QAbstractSocket::ConnectedState) [[likely]] {
            client->write(output);
        }
    }
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
    DisplayImage(const QString &connectorName, const QString &format, QObject *parent = nullptr)
        : Display(connectorName, parent)
        , m_format(format.toLocal8Bit())
    {}

private:
    const QByteArray m_format;

    void processImage(const QImage &result) override
    {
        Packets::ServerImage servImg{};
        servImg.format.data = m_format;
        {
            QBuffer buf(&servImg.data.data);
            buf.open(QIODevice::WriteOnly);
            result.save(&buf, m_format, Parameters::instance.qualityLevel);
            buf.close();
        }

        sendData(std::move(Packets::Writer::generate(servImg)));
    }
};

class DisplayH265 : public Display
{
public:
    DisplayH265(const QString &connectorName, QObject *parent = nullptr)
        : Display(connectorName, parent)
        , m_encoder([this](const uint8_t *data, const size_t size, const int64_t pts) { this->h265dataForward(data, size, pts); },
                    1000 / Settings::frameMSecsInterval)
    {}

private:
    void processImage(const QImage &result) override
    {
        m_encoder.push_image(result.bits(), result.width(), result.height(), result.format(), result.bytesPerLine());
    }

    void h265dataForward(const uint8_t *data, const size_t size, const int64_t pts)
    {
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

Display *generateNewDisplay(const QString &connectorName, QObject *parent)
{
    switch (Parameters::instance.streamFormat) {
    case Opts::DisplayStreamType::h265: {
        return new DisplayH265(connectorName, parent);
    }
    case Opts::DisplayStreamType::jpg: {
        return new DisplayImage(connectorName, "JPG", parent);
    }
    case Opts::DisplayStreamType::png: {
        return new DisplayImage(connectorName, "PNG", parent);
    }
    case Opts::DisplayStreamType::webp: {
        return new DisplayImage(connectorName, "WEBP", parent);
    }
    }

    // [TODO] Generate an error message.
    return nullptr;
}
