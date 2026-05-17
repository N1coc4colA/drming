#include "display.h"

#include <QBuffer>
#include <QDebug>
#include <QTcpSocket>
#include <QtEndian>

#include "parameters.h"

Display::Display(const QString &connectorName, QObject *parent)
    : QObject(parent)
    , m_reader(connectorName)
{
    QObject::connect(&m_timer, &QTimer::timeout, this, &Display::forward);

    m_timer.setInterval(60);
}

void Display::setClient(QTcpSocket *client)
{
    m_client = client;
    connect(client, &QTcpSocket::disconnected, this, &Display::onDisconnected);
    onConnected();
}

void Display::forward()
{
    VkmsFrameBuffer fb{};
    if (!m_reader.getVkmsFrameBuffer(fb)) {
        if (!primaryFailureNotice) {
            primaryFailureNotice = true;
            qCritical() << "Failed to get primary";
        }

        return;
    }

    primaryFailureNotice = false;

    CursorFrameBuffer cursorFb{};
    const bool hasCursor = m_reader.getCursorFrameBuffer(cursorFb, fb);

    QByteArray output{};
    QImage result = m_reader.imageFromFrameBuffer(static_cast<const uint8_t *>(fb.data), fb.width, fb.height, fb.stride, fb.format);
    if (hasCursor) {
        result = m_reader.compositeWithCursor(result, cursorFb);
    }

    {
        QBuffer buf(&output);
        buf.open(QIODevice::WriteOnly);
        // high quality = 90, trade CPU vs size
        result.save(&buf, "JPEG", Parameters::instance.jpegCompression);
        buf.close();

        const qsizetype size = qToBigEndian(static_cast<qsizetype>(output.size()));
        output.prepend(reinterpret_cast<const char *>(&size), sizeof(size));
    }

    m_reader.releaseVkmsFrameBuffer(fb);

    if (m_client && m_client->state() == QAbstractSocket::ConnectedState) {
        m_client->write(output);
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
}
