#include "networkclient.h"

#include <QDebug>
#include <QDtls>
#include <QSslConfiguration>
#include <QTimer>
#include <qendian.h>

#include "display.h"

void NetworkClient::processPacket(const Packets::HeartBeat &)
{
    notifyHeartBeat();
}

void NetworkClient::processPacket(const Packets::Reinit &)
{
    m_display->reinit();
}

void NetworkClient::processPacket(const Packets::RequestClientResolution &)
{
    m_display->requireResolutionInformation();
}

NetworkClient::NetworkClient(
    QDtls *dtls, const QHostAddress &address, const quint16 port, QUdpSocket &sharedSocket, QMutex &networkMutex, QObject *parent)
    : QObject(parent)
    , Packets::Parser<NetworkClient>(*this)
    , m_dtls(dtls)
    , m_address(address)
    , m_port(port)
    , m_sharedSocket(sharedSocket)
    , m_networkMutex(networkMutex)
    , m_timer(new QTimer(this))
{
    m_dtls->setParent(this); // take ownership

    m_timer->setTimerType(Qt::VeryCoarseTimer);
    connect(m_timer, &QTimer::timeout, this, &NetworkClient::checkHeartBeat);
    m_timer->setInterval(Settings::inactivityTimeout);
}

NetworkClient::~NetworkClient()
{
    // dtls deleted automatically via parent
}

void NetworkClient::incomingEncryptedData(const QByteArray &encrypted)
{
    // Decrypt using the peer address/port – QDtls::decryptDatagram overload
    auto plaintext = m_dtls->decryptDatagram(&m_sharedSocket, encrypted);
    if (plaintext.isEmpty()) {
        if (m_dtls->dtlsError() != QDtlsError::NoError) {
            qWarning() << "DTLS decrypt error:" << m_dtls->dtlsErrorString();
            // If fatal, close connection
            if (m_dtls->dtlsError() == QDtlsError::RemoteClosedConnectionError) {
                close();
            }
        }

        return;
    }

    if (plaintext.size() < static_cast<qsizetype>(sizeof(Packets::Ordering) * 2 + sizeof(Packets::Timestamp))) {
        qWarning() << "Invalid DTLS decrypted packet size has been received.";
        return;
    }

    const auto ts = qFromBigEndian(*reinterpret_cast<Packets::Timestamp *>(plaintext.first(sizeof(Packets::Timestamp)).data()));
    const auto sizing = qFromBigEndian(*reinterpret_cast<Packets::Ordering *>(
        plaintext.slice(static_cast<qsizetype>(sizeof(Packets::Timestamp))).first(sizeof(Packets::Ordering)).data()));
    const auto order = qFromBigEndian(*reinterpret_cast<Packets::Ordering *>(
        plaintext.slice(static_cast<qsizetype>(sizeof(Packets::Ordering))).first(sizeof(Packets::Ordering)).data()));

    m_jitterBuffer.pushChunk(ts, order, sizing, plaintext.slice(static_cast<qsizetype>(sizeof(Packets::Ordering))));

    while (const auto v = m_jitterBuffer.pullComplete()) {
        addData(std::move(*v));
    }
}

qint64 NetworkClient::write(const QByteArray &data)
{
    if (!m_dtls) {
        return -1;
    }

    static constexpr auto innerSize = Settings::dtlsChunkSize - sizeof(Packets::Ordering) * 2 - sizeof(Packets::Timestamp);
    const auto size = data.size();
    const Packets::Ordering chunksCount = (size / innerSize) + (size % innerSize ? 1 : 0);
    const auto chunks_be = qToBigEndian(chunksCount);

    Packets::Ordering order = 0;
    qsizetype offset = 0;
    while (offset < size) {
        auto chunk = data.mid(offset, innerSize);
        const auto ts_be = qToBigEndian(m_ts);
        const auto order_be = qToBigEndian(order);
        const auto chunkSize = chunk.size();
        chunk.prepend(reinterpret_cast<const char *>(&order_be), sizeof(order_be));
        chunk.prepend(reinterpret_cast<const char *>(&chunks_be), sizeof(chunks_be));
        chunk.prepend(reinterpret_cast<const char *>(&ts_be), sizeof(ts_be));

        qint64 ret;
        {
            QMutexLocker lock(&m_networkMutex);
            ret = m_dtls->writeDatagramEncrypted(&m_sharedSocket, chunk);
        }

        if (ret < 0) {
            const auto err = m_dtls->dtlsError();
            switch (err) {
            case QDtlsError::RemoteClosedConnectionError: {
                close();
                break;
            }
            case QDtlsError::UnderlyingSocketError:
            case QDtlsError::NoError: {
                continue;
            }
            default: {
                qWarning() << "DTLS write error on" << m_ts << order << static_cast<int>(err) << m_dtls->dtlsErrorString();
            }
            }

            m_ts++;
            m_ts = m_ts % Settings::simultaneousPendings;

            return -1;
        }

        offset += chunkSize;
        order++;
    }

    m_ts++;
    m_ts = m_ts % Settings::simultaneousPendings;

    return offset;
}

void NetworkClient::close()
{
    // Notify server that we are closing; the server will remove us.
    Q_EMIT disconnected();
}

QAbstractSocket::SocketState NetworkClient::state() const
{
    if (!m_dtls) {
        return QAbstractSocket::UnconnectedState;
    }

    return m_dtls->isConnectionEncrypted() ? QAbstractSocket::ConnectedState : QAbstractSocket::ConnectingState;
}

QByteArray NetworkClient::digest() const
{
    if (!m_dtls) {
        return {};
    }

    return m_dtls->dtlsConfiguration().peerCertificate().digest();
}

void NetworkClient::notifyHeartBeat()
{
    m_timer->start();
    m_lastHeartBeat = std::chrono::system_clock::now();
}

void NetworkClient::checkHeartBeat()
{
    if ((std::chrono::system_clock::now() - m_lastHeartBeat) > systemTimeout) {
        m_timer->stop();
        // Heartbeat timeout – close connection
        close();
    }
}
