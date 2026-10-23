#ifndef SERVER_H
#define SERVER_H

#include <QHostAddress>
#include <QMap>
#include <QMutex>
#include <QObject>
#include <QPair>
#include <QUdpSocket>

#include "networkclient.h"


class QDtls;

struct PeerKey
{
    QAbstractSocket::NetworkLayerProtocol proto = QAbstractSocket::UnknownNetworkLayerProtocol;
    quint32 v4 = 0;
    Q_IPV6ADDR v6{};
    quint16 port = 0;
    QString scopeId{};

    inline bool operator==(const PeerKey &o) const noexcept
    {
        return proto == o.proto
               && port == o.port
               && v4 == o.v4
               && scopeId == o.scopeId
               && std::memcmp(&v6, &o.v6, sizeof v6) == 0;
    }

    inline bool operator!=(const PeerKey &o) const noexcept { return !(*this == o); }

    friend inline auto qHash(const PeerKey &k, const size_t seed = 0) noexcept
    {
        return qHashMulti(
            seed, k.proto, k.v4, k.port, k.scopeId,
            qFromBigEndian<quint64>(k.v6.c), qFromBigEndian<quint64>(k.v6.c + 8)
        );
    }
};

/**
 * Accepts secure client connections over IPv4 and IPv6.
 *
 * Each peer first completes a DTLS handshake and is then promoted to a
 * NetworkClient that owns heartbeat and packet processing for that peer.
 */
class Server : public QObject
{
    Q_OBJECT

public:
    explicit Server(QObject *parent = nullptr);

    /// Binds both UDP sockets and begins accepting DTLS handshakes.
    bool listen(const QHostAddress &address4, const QHostAddress &address6, quint16 port = 0);
    /// Stops accepting peers and closes all active connections.
    void close();

    [[nodiscard]] virtual bool hasClient() const { return !m_clients.isEmpty(); }

Q_SIGNALS:
    void noClient();
    void clientConnected(NetworkClient *client);

public Q_SLOTS:
    void broadcast(const QByteArray &data);

private Q_SLOTS:
    void onDatagramReceived4();
    void onDatagramReceived6();
    void onClientDisconnected();

private:
    QPair<QUdpSocket, QMutex> m_socket4{};
    QPair<QUdpSocket, QMutex> m_socket6{};

    // Active clients: peer key -> NetworkClientDtls*
    QHash<PeerKey, NetworkClient *> m_clients{};
    // Pending handshakes: peer key -> QDtls*
    QHash<PeerKey, QDtls *> m_pendingHandshakes{};

    QSslConfiguration m_sslConfig{};

    PeerKey peerKey(const QHostAddress &addr, quint16 port) const;
    void removeClient(NetworkClient *client);

    void processSocketPendings(QPair<QUdpSocket, QMutex> &pair);
};

#endif // SERVER_H
