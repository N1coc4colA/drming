#include "networklink.h"

#include <QBuffer>
#include <QDebug>
#include <QtEndian>

NetworkLink::NetworkLink(QObject *parent)
    : QObject{parent}
    , m_socket(new QTcpSocket(this))
{
    QAbstractSocket::connect(m_socket, &QTcpSocket::connected, this, &NetworkLink::onConnected);
    QAbstractSocket::connect(m_socket, &QTcpSocket::disconnected, this, &NetworkLink::onDisconnected);
    QAbstractSocket::connect(m_socket, &QTcpSocket::readyRead, this, &NetworkLink::onDataAvailable);
    QAbstractSocket::connect(m_socket, &QTcpSocket::errorOccurred, this, &NetworkLink::onError);

    m_socket->setSocketOption(QAbstractSocket::LowDelayOption, 1);  // disables Nagle
    m_socket->setSocketOption(QAbstractSocket::KeepAliveOption, 1); // optional: enable keepalive
}

NetworkLink::~NetworkLink()
{
    m_socket->close();
}

void NetworkLink::close()
{
    if (m_socket->state() == QAbstractSocket::UnconnectedState) {
        return;
    }

    m_socket->close();
}

void NetworkLink::connect(const QString &address, const int port)
{
    if (m_socket->state() != QAbstractSocket::UnconnectedState) {
        return;
    }

    qInfo() << "Connecting to:" << address << port;
    m_socket->connectToHost(address, port);
}

void NetworkLink::onError(const QAbstractSocket::SocketError error)
{
    qWarning() << "Connection error occurred: " << error;
    if (m_socket) {
        Q_EMIT NetworkLink::error(m_socket->errorString());
    } else {
        Q_EMIT NetworkLink::error(tr("A network error occurred."));
    }
}

void NetworkLink::onConnected()
{
    qInfo() << "Connected";
}

void NetworkLink::onDisconnected()
{
    m_socket->close();
    m_buffer.clear();
}

void NetworkLink::onDataAvailable()
{
    static constexpr qsizetype headerSize = sizeof(qsizetype);
    m_buffer += m_socket->readAll();

    if (m_imageSize == 0) {
        if (m_buffer.size() < headerSize) {
            return;
        }

        QDataStream reader(m_buffer);
        reader.setByteOrder(QDataStream::BigEndian);
        reader >> m_imageSize;
        m_buffer.remove(0, headerSize);
    }

    if (m_buffer.size() < m_imageSize) {
        return;
    }

    const auto img = QImage::fromData(m_buffer, "JPEG");

    m_buffer.remove(0, m_imageSize);
    m_imageSize = 0;

    Q_EMIT imageReady(img);
}
