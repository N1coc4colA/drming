#include "networkclient.h"

#include <QDtls>
#include <QSslConfiguration>

#include "../settings.h"

NetworkClient::NetworkClient(QAbstractSocket *socket, QObject *parent)
    : QObject(parent)
    , m_socket(socket)
{
    m_socket->setParent(this);

    connect(m_socket, &QAbstractSocket::disconnected, this, &NetworkClient::disconnected);
}

NetworkClientDtls::NetworkClientDtls(QDtls *dtls, QUdpSocket *socket, QObject *parent)
    : NetworkClient(socket, parent)
    , m_dtls(dtls)
{}

NetworkClientSsl::NetworkClientSsl(QSslSocket *socket, QObject *parent)
    : NetworkClient(socket, parent)
{}

qint64 NetworkClientDtls::write(const QByteArray &data)
{
    auto *udp = qobject_cast<QUdpSocket *>(m_socket);
    if (!m_dtls || !udp) [[unlikely]] {
        return -1;
    }

    qsizetype offset = 0;
    while (offset < data.size()) {
        const auto chunk = data.mid(offset, Settings::dtlsChunkSize);

        if (m_dtls->writeDatagramEncrypted(udp, chunk) < 0) [[unlikely]] {
            const auto err = m_dtls->dtlsError();
            if (err != QDtlsError::NoError && err != QDtlsError::UnderlyingSocketError) {
                qWarning() << "DTLS Error" << static_cast<int>(err) << ":" << m_dtls->dtlsErrorString();
            }
            return -1;
        }

        offset += chunk.size();
    }

    return data.size();
}

/*qint64 NetworkClientDtls::write(const QByteArray &data)
{
    auto socket = qobject_cast<QUdpSocket *>(m_socket);
    if (!m_dtls || !socket) [[unlikely]] {
        return -1;
    }

    constexpr qsizetype chunkSize = Settings::dtlsChunkSize;
    qsizetype offset = 0;
    while (offset < data.size()) {
        const auto chunk = data.mid(offset, chunkSize);

        if (m_dtls->writeDatagramEncrypted(socket, chunk) < 0) [[unlikely]] {
            const auto err = m_dtls->dtlsError();
            if (err != QDtlsError::NoError && err != QDtlsError::UnderlyingSocketError) {
                qWarning() << "DTLS Error" << static_cast<int>(err) << ":" << m_dtls->dtlsErrorString();

                break;
            }
        } else {
            offset += chunkSize;
        }
    }

    return m_dtls->writeDatagramEncrypted(socket, data);
}*/

qint64 NetworkClientSsl::write(const QByteArray &data)
{
    if (!m_socket) [[unlikely]] {
        return -1;
    }

    // [TODO] Handle this stuff more gracefully. Would need a loop to ensure everything's been written 'til the end.
    const auto ret = m_socket->write(data);

    m_socket->waitForBytesWritten();

    return ret;
}

QByteArray NetworkClientDtls::digest() const
{
    return m_dtls->dtlsConfiguration().peerCertificate().digest();
}

QByteArray NetworkClientSsl::digest() const
{
    return qobject_cast<QSslSocket *>(m_socket)->peerCertificate().digest();
}
