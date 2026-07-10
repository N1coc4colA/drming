#include "networkclient.h"

#include <QDtls>

NetworkClient::NetworkClient(QHostAddress addr, const quint16 port, QDtls *dtls, QUdpSocket *socket, QObject *parent)
    : QObject(parent)
    , m_addr(std::move(addr))
    , m_port(port)
    , m_dtls(dtls)
    , m_socket(socket)
{}

qint64 NetworkClient::write(const QByteArray &data)
{
    if (!m_dtls || !m_socket) {
        [[unlikely]];

        return -1;
    }

    return m_dtls->writeDatagramEncrypted(m_socket, data);
}
