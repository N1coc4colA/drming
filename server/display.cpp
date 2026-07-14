#include "display.h"

#include <QBuffer>
#include <QDtls>
#include <QtEndian>

#include "../parser.h"
#include "../settings.h"

#include "ffmpeg.h"
#include "parameters.h"

Display::Display(const QString &connectorName, QObject *parent)
    : QObject(parent)
    , m_reader(connectorName)
{
    connect(&m_timer, &QTimer::timeout, this, &Display::forward);
    m_timer.setInterval(Settings::frameMSecsInterval);
}

void Display::setClient(NetworkClient *client)
{
    m_client = client;
    if (m_client) {
        connect(m_client, &NetworkClient::disconnected, this, &Display::onDisconnected);
    }

    onConnected();
}

void Display::forward()
{
    VkmsFrameBuffer fb{};
    if (!m_reader.getVkmsFrameBuffer(fb)) {
        [[unlikely]];

        if (!primaryFailureNotice) {
            primaryFailureNotice = true;
            qCritical() << "Failed to get primary";
        }

        return;
    }

    primaryFailureNotice = false;

    CursorFrameBuffer cursorFb{};
    const auto hasCursor = DisplayReader::getCursorFrameBuffer(cursorFb, fb);

    if (!m_vkmsFrameDescriptor.has_value()) {
        [[unlikely]];

        m_vkmsFrameDescriptor = DrmFormat::resolve(fb.format);

        if (m_vkmsFrameDescriptor->qtFormat == QImage::Format_Invalid) {
            [[unlikely]];

            char a, b, c, d;
            Drm::split_fourcc(fb.format, a, b, c, d);

            qWarning() << "Image format for frame is invalid:" << fb.format << ";" << a << b << c << d;

            if (m_client) {
                m_client->disconnect();
            }
            return;
        }
    }

    if (m_vkmsFrameDescriptor->qtFormat == QImage::Format_Invalid) {
        [[unlikely]];
        return;
    }

    auto result = DisplayReader::imageFromFrameBuffer(static_cast<const uint8_t *>(fb.data), fb.width, fb.height, fb.stride, m_vkmsFrameDescriptor.value());
    if (hasCursor) {
        [[likely]];

        if (!m_cursorFrameDescriptor.has_value()) {
            [[unlikely]];

            m_cursorFrameDescriptor = DrmFormat::resolve(cursorFb.format);

            if (m_cursorFrameDescriptor->qtFormat == QImage::Format_Invalid) {
                [[unlikely]];

                char a, b, c, d;
                Drm::split_fourcc(cursorFb.format, a, b, c, d);

                qWarning() << "Image format for frame is invalid:" << cursorFb.format << ";" << a << b << c << d;

                if (m_client) {
                    // [TODO] Generate error on failure
                    m_client->disconnect();
                }
                return;
            }
        }

        if (m_cursorFrameDescriptor->qtFormat == QImage::Format_Invalid) {
            [[unlikely]];

            return;
        }

        DisplayReader::compositeWithCursor(result, cursorFb, m_cursorFrameDescriptor.value());
    }

    processImage(result);
    DisplayReader::releaseVkmsFrameBuffer(fb);
}

void Display::onConnected()
{
    m_timer.start();
}

void Display::onDisconnected()
{
    m_timer.stop();
    m_client = nullptr;
    Q_EMIT nowFree(this);
}

void Display::sendData(QByteArray output)
{
    if (m_client && m_client->state() == QAbstractSocket::ConnectedState) {
        [[likely]];

        m_client->write(output);
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
        {
            QBuffer buf(&servImg.data);
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
        , m_encoder([this](const uint8_t *data, size_t size, int64_t pts) { this->h265dataForward(data, size, pts); },
                    1000 / Settings::frameMSecsInterval)
    {}

private:
    void processImage(const QImage &result) override
    {
        m_encoder.push_image(result.bits(), result.width(), result.height(), result.format(), result.bytesPerLine());
    }

    void h265dataForward(const uint8_t *data, const size_t size, const int64_t pts)
    {
        Packets::ServerStream stm{.data = QByteArray("\x00\x00\x00\x01").append(reinterpret_cast<const char *>(data), static_cast<qsizetype>(size))};
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
}
