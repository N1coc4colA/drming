#include "display.h"

#include <QBuffer>
#include <QDebug>
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
    const auto hasCursor = m_reader.getCursorFrameBuffer(cursorFb, fb);

    Packets::ServerImage servImg{};
    auto result = m_reader.imageFromFrameBuffer(static_cast<const uint8_t *>(fb.data), fb.width, fb.height, fb.stride, fb.format);
    if (hasCursor) {
        [[likely]];

        result = m_reader.compositeWithCursor(result, cursorFb);
    }

    {
        QBuffer buf(&servImg.data);
        buf.open(QIODevice::WriteOnly);
        result.save(&buf, Settings::frameImageFormat, Parameters::instance.qualityLevel);
        buf.close();
    }

    auto output = Packets::Writer::generate(servImg);
    m_reader.releaseVkmsFrameBuffer(fb);

    if (m_client && m_client->state() == QAbstractSocket::ConnectedState) {
        constexpr qsizetype chunkSize = Settings::dtlsChunkSize;
        for (qsizetype offset = 0; offset < output.size(); offset += chunkSize) {
            const auto chunk = output.mid(offset, chunkSize);
            if (m_client->write(chunk) < 0) {
                [[unlikely]];

                qWarning() << "Failed to write DTLS image chunk";
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
