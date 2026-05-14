#include "networklink.h"

#include <QBuffer>
#include <QDebug>
#include <QtEndian>

#include <iostream>

NetworkLink::NetworkLink(QObject *parent)
    : QObject{parent}
    , m_socket(new QTcpSocket(this))
{
    QAbstractSocket::connect(m_socket, &QTcpSocket::connected, this, &NetworkLink::onConnected);
    QAbstractSocket::connect(m_socket, &QTcpSocket::disconnected, this, &NetworkLink::onDisconnected);
    QAbstractSocket::connect(m_socket, &QTcpSocket::readyRead, this, &NetworkLink::onDataAvailable);
    QAbstractSocket::connect(m_socket, &QTcpSocket::errorOccurred, [](const QAbstractSocket::SocketError error) {
        std::cout << "Connection error occurred: " << error << '\n';
    });
    QAbstractSocket::connect(m_socket, &QTcpSocket::stateChanged, [](const QAbstractSocket::SocketState socketState) {
        std::cout << "State changed: " << socketState << std::endl;
    });
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

    qDebug() << "Connecting to:" << address << port;
    m_socket->connectToHost(address, port);
}

void NetworkLink::onError(const QAbstractSocket::SocketError error)
{
    Q_EMIT NetworkLink::error("Something happened");
}

void NetworkLink::onConnected()
{
    std::cout << "Connected\n";
}

void NetworkLink::onDisconnected()
{
    m_socket->close();
    m_buffer.clear();
}

void NetworkLink::onDataAvailable()
{
    static constexpr qsizetype headerSize = sizeof(quint16) + sizeof(quint32) * 3;
    m_buffer += m_socket->readAll();

    if (m_imageSize == 0) {
        if (m_buffer.size() < headerSize) {
            return;
        }

        QDataStream reader(m_buffer);
        reader.setByteOrder(QDataStream::BigEndian);
        reader >> m_format;
        reader >> m_width;
        reader >> m_height;
        reader >> m_stride;

        m_imageSize = static_cast<qsizetype>(m_stride) * static_cast<qsizetype>(m_height);
        m_buffer.remove(0, headerSize);
    }

    if (m_buffer.size() < m_imageSize) {
        return;
    }

    // We need to detach the image's buffer too.
    const auto img = QImage(reinterpret_cast<const uchar *>(m_buffer.constData()),
                            static_cast<int>(m_width),
                            static_cast<int>(m_height),
                            static_cast<int>(m_stride),
                            static_cast<QImage::Format>(m_format))
                         .copy();

    m_buffer.remove(0, m_imageSize);
    m_imageSize = 0;

    Q_EMIT imageReady(img);
}
