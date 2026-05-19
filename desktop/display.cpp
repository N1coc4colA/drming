#include "display.h"

#include <QBuffer>
#include <QDebug>
#include <QTcpSocket>
#include <QtEndian>

#include "../Parser.h"
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

    Packets::ServerImage servImg{};
    QImage result = m_reader.imageFromFrameBuffer(static_cast<const uint8_t *>(fb.data), fb.width, fb.height, fb.stride, fb.format);
    if (hasCursor) {
        result = m_reader.compositeWithCursor(result, cursorFb);
    }

    {
        QBuffer buf(&servImg.data);
        buf.open(QIODevice::WriteOnly);
        result.save(&buf, "JPEG", Parameters::instance.jpegCompression);
        buf.close();
    }

    QByteArray output = Packets::Writer::generate(servImg);
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
    Q_EMIT nowFree(this);
}
