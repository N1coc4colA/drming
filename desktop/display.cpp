#include "display.h"

#include <QBuffer>
#include <QDtls>
#include <QtEndian>

#include "../parser.h"
#include "../settings.h"

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

    Packets::ServerImage servImg{};

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

        result = DisplayReader::compositeWithCursor(result, cursorFb, m_cursorFrameDescriptor.value());
    }

    {
        QBuffer buf(&servImg.data);
        buf.open(QIODevice::WriteOnly);
        result.save(&buf, Settings::frameImageFormat, Parameters::instance.qualityLevel);
        buf.close();
    }

    const auto output = Packets::Writer::generate(servImg);
    DisplayReader::releaseVkmsFrameBuffer(fb);

    if (m_client && m_client->state() == QAbstractSocket::ConnectedState) {
        [[likely]];

        constexpr qsizetype chunkSize = Settings::dtlsChunkSize;
        for (qsizetype offset = 0; offset < output.size(); offset += chunkSize) {
            const auto chunk = output.mid(offset, chunkSize);
            if (m_client->write(chunk) < 0) {
                [[unlikely]];

                qWarning() << "Failed to write DTLS image chunk";
                const auto err = m_client->dtls()->dtlsError();
                if (err != QDtlsError::NoError) {
                    qWarning() << "DTLS Error" << static_cast<int>(err) << ":" << m_client->dtls()->dtlsErrorString();
                }
                break;
            }
        }
    }
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
