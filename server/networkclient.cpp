#include "networkclient.h"

#include <QDtls>

#include "../settings.h"

NetworkClient::NetworkClient(QAbstractSocket *socket, QObject *parent)
    : QObject(parent)
    , m_socket(socket)
{}

NetworkClientDtls::NetworkClientDtls(QDtls *dtls, QUdpSocket *socket, QObject *parent)
    : NetworkClient(socket, parent)
    , m_dtls(dtls)
{}

NetworkClientSsl::NetworkClientSsl(QSslSocket *socket, QObject *parent)
    : NetworkClient(socket, parent)
{}

qint64 NetworkClientDtls::write(const QByteArray &data)
{
    if (!m_dtls || !m_socket) {
        [[unlikely]];

        return -1;
    }

    constexpr qsizetype chunkSize = Settings::dtlsChunkSize;
    qsizetype offset = 0;
    while (offset < data.size()) {
        const auto chunk = data.mid(offset, chunkSize);

        if (m_dtls->writeDatagramEncrypted(qobject_cast<QUdpSocket *>(m_socket), chunk) < 0) {
            [[unlikely]];

            const auto err = m_dtls->dtlsError();
            if (err != QDtlsError::NoError && err != QDtlsError::UnderlyingSocketError) {
                qWarning() << "DTLS Error" << static_cast<int>(err) << ":" << m_dtls->dtlsErrorString();

                break;
            }
        } else {
            offset += chunkSize;
        }
    }

    return m_dtls->writeDatagramEncrypted(qobject_cast<QUdpSocket *>(m_socket), data);
}

qint64 NetworkClientSsl::write(const QByteArray &data)
{
    if (!m_socket) {
        [[unlikely]];

        return -1;
    }

    // [TODO] Handle this stuff more gracefully. Would need a loop to ensure everything's been written 'til the end.
    const auto ret = m_socket->write(data);

    m_socket->waitForBytesWritten();

    return ret;
}
