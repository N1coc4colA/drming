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

NetworkClientDtls::NetworkClientDtls(QDtls *dtls, const QHostAddress &address, quint16 port,
                                     QUdpSocket *sharedSocket, QObject *parent)
    : NetworkClient(parent)
    , m_dtls(dtls)
    , m_address(address)
    , m_port(port)
    , m_sharedSocket(sharedSocket)
    , m_timer(new QTimer(this))
{
    m_dtls->setParent(this); // take ownership

    m_timer->setTimerType(Qt::VeryCoarseTimer);
    connect(m_timer, &QTimer::timeout, this, &NetworkClientDtls::checkHeartBeat);
    m_timer->setInterval(Settings::inactivityTimeout);
}

NetworkClientDtls::~NetworkClientDtls()
{
    // dtls deleted automatically via parent
}

void NetworkClientDtls::incomingEncryptedData(const QByteArray &encrypted)
{
    // Decrypt using the peer address/port – QDtls::decryptDatagram overload
    auto plaintext = m_dtls->decryptDatagram(m_sharedSocket, encrypted);
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

    if (plaintext.size() < static_cast<qsizetype>(sizeof(Packets::Ordering) + sizeof(Packets::Timestamp))) {
        qWarning() << "Invalid DTLS decrypted packet size has been received.";
        return;
    }

    const auto ts = qFromBigEndian(*reinterpret_cast<Packets::Timestamp *>(plaintext.first(sizeof(Packets::Timestamp)).data()));
    assert(plaintext.size() >= static_cast<qsizetype>(sizeof(Packets::Timestamp)));
    plaintext.slice(static_cast<qsizetype>(sizeof(Packets::Timestamp)));
    const auto order = qFromBigEndian(*reinterpret_cast<Packets::Ordering *>(plaintext.first(sizeof(Packets::Ordering)).data()));
    assert(plaintext.size() >= static_cast<qsizetype>(sizeof(Packets::Ordering)));
    plaintext.slice(static_cast<qsizetype>(sizeof(Packets::Ordering)));

    if (ts == 0) {
        m_pendingTimestamp = -1;
    }

    const auto pos = static_cast<std::size_t>(order);
    // If we get back to 0, it means we have a new data line incoming. If it's higher but already preset, it just means we may not have the 0th one yet, but we still need to push.
    if (order == 0 || m_pendingTimestamp < ts) {
        // Check if we got all packets, meaning it should be valid.
        if (!std::any_of(m_pendings.cbegin(), m_pendings.cend(), [](const auto p) { return !p.has_value(); })) {
            const auto total = std::accumulate(m_pendings.cbegin(), m_pendings.cend(), qsizetype(0), [](auto curr, const auto arr) {
                return curr += (*arr).size();
            });

            // Then we just gotta sum it all up.
            QByteArray out;
            out.reserve(total);
            for (const auto &arr : std::as_const(m_pendings)) {
                out += (*arr);
            }

            clear();
            addData(std::move(out));
        }

        m_pendingTimestamp = ts;
        m_pendings.clear();
    }

    if (m_pendings.size() < (pos + 1)) {
        m_pendings.resize(pos + 1);
    }

    // We need to slice as the ordering is not part of the information wanted by the application.
    m_pendings[static_cast<std::size_t>(order)] = plaintext;
}

qint64 NetworkClientDtls::write(const QByteArray &data)
{
    if (!m_dtls || !m_sharedSocket) {
        return -1;
    }

    static constexpr auto innerSize = Settings::dtlsChunkSize - sizeof(Packets::Ordering) - sizeof(Packets::Timestamp);
    const auto size = data.size();

    qDebug() << "Pushing data line of" << ((size / innerSize) + (size % innerSize ? 1 : 0));

    Packets::Ordering order = 0;
    qsizetype offset = 0;
    while (offset < size) {
        auto chunk = data.mid(offset, innerSize);
        const auto ts_be = qToBigEndian(m_ts);
        const auto order_be = qToBigEndian(order);
        chunk.prepend(reinterpret_cast<const char *>(&order_be), sizeof(order_be));
        chunk.prepend(reinterpret_cast<const char *>(&ts_be), sizeof(ts_be));

        if (m_dtls->writeDatagramEncrypted(m_sharedSocket, chunk) < 0) {
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

        offset += chunk.size();
        order++;
    }

    m_ts++;
    m_ts = m_ts % Settings::simultaneousPendings;

    return offset;
}

void NetworkClientDtls::close()
{
    // Notify server that we are closing; the server will remove us.
    Q_EMIT disconnected();
}

QAbstractSocket::SocketState NetworkClientDtls::state() const
{
    if (!m_dtls)
        return QAbstractSocket::UnconnectedState;
    return m_dtls->isConnectionEncrypted() ? QAbstractSocket::ConnectedState : QAbstractSocket::ConnectingState;
}

QByteArray NetworkClientDtls::digest() const
{
    if (!m_dtls)
        return {};
    return m_dtls->dtlsConfiguration().peerCertificate().digest();
}

void NetworkClientDtls::notifyHeartBeat()
{
    m_timer->start();
    m_lastHeartBeat = std::chrono::system_clock::now();
}

void NetworkClientDtls::checkHeartBeat()
{
    if ((std::chrono::system_clock::now() - m_lastHeartBeat) > systemTimeout) {
        m_timer->stop();
        // Heartbeat timeout – close connection
        close();
    }
}

NetworkClientSsl::NetworkClientSsl(QSslSocket *socket, QObject *parent)
    : NetworkClient(parent)
    , m_socket(socket)
{
    m_socket->setParent(this);
    connect(m_socket, &QSslSocket::readyRead, this, [this]() {
        addData(m_socket->readAll());
    });
    connect(m_socket, &QSslSocket::disconnected, this, &NetworkClientSsl::disconnected);
}

qint64 NetworkClientSsl::write(const QByteArray &data)
{
    if (!m_socket) {
        return -1;
    }

    const auto ret = m_socket->write(data);
    m_socket->flush();
    return ret;
}

QByteArray NetworkClientSsl::digest() const
{
    if (!m_socket) {
        return {};
    }

    return m_socket->peerCertificate().digest();
}
