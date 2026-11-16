#ifndef SERVER_H
#define SERVER_H

#include <QHostAddress>
#include <QObject>

#include "networkclient.h"

class QDtls;

/// Uniquely identifies a network peer by protocol, address, port, and scope.
struct PeerKey
{
    /// Network layer protocol (IPv4 or IPv6).
    QAbstractSocket::NetworkLayerProtocol proto = QAbstractSocket::UnknownNetworkLayerProtocol;
    /// IPv4 address in host byte order.
    quint32 v4 = 0;
    /// IPv6 address structure.
    Q_IPV6ADDR v6{};
    /// Remote UDP port number.
    quint16 port = 0;
    /// IPv6 interface scope identifier.
    QString scopeId{};

    bool operator==(const PeerKey &o) const noexcept
    {
        return proto == o.proto && port == o.port && v4 == o.v4 && scopeId == o.scopeId
            && std::memcmp(&v6, &o.v6, sizeof v6) == 0;
    }

    bool operator!=(const PeerKey &o) const noexcept
    {
        return !(*this == o);
    }

    friend auto qHash(const PeerKey &k, const size_t seed = 0) noexcept
    {
        return qHashMulti(
            seed,
            k.proto,
            k.v4,
            k.port,
            k.scopeId,
            qFromBigEndian<quint64>(k.v6.c),
            qFromBigEndian<quint64>(k.v6.c + 8)
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
    ~Server() override;

    /// Binds both UDP sockets and begins accepting DTLS handshakes.
    bool listen(const QHostAddress &address4, const QHostAddress &address6, quint16 port = 0);
    /// Stops accepting peers and closes all active connections.
    void close();

    /// Returns true if at least one client is currently connected.
    [[nodiscard]] virtual bool hasClient() const
    {
        return !m_clients.isEmpty();
    }

    /// Sets the QObject parent assigned to new NetworkClient instances.
    void setClientsOwner(QObject *);
    /// Returns the QObject parent assigned to new NetworkClient instances.
    QObject *getClientsOwner()
    {
        return m_clientsOwner;
    }
    [[nodiscard]] const QObject *getClientsOwner() const
    {
        return m_clientsOwner;
    }

Q_SIGNALS:
    /// Emitted when the last connected client disconnects.
    void noClient();
    /// Emitted when a new client completes the DTLS handshake.
    void clientConnected(NetworkClient *client);

public Q_SLOTS:
    /// Broadcasts data datagram to all active clients.
    void broadcast(const QByteArray &data);

private Q_SLOTS:
    void onDatagramReceived4();
    void onDatagramReceived6();
    void onClientDisconnected();

private:
    /// IPv4 UDP socket and its synchronization mutex.
    QPair<QUdpSocket, QMutex> m_socket4{};
    /// IPv6 UDP socket and its synchronization mutex.
    QPair<QUdpSocket, QMutex> m_socket6{};

    /// Map of active connected clients indexed by peer key.
    QHash<PeerKey, NetworkClient *> m_clients{};
    /// Map of pending DTLS handshake sessions indexed by peer key.
    QHash<PeerKey, QDtls *> m_pendingHandshakes{};

    /// SSL configuration used for DTLS server handshakes.
    QSslConfiguration m_sslConfig{};

    /// Parent QObject assigned to newly created NetworkClient objects.
    QObject *m_clientsOwner = nullptr;

    /// Removes and cleans up the specified client.
    void removeClient(NetworkClient *client);

    /// Reads and processes pending datagrams from the given socket pair.
    void processSocketPendings(QPair<QUdpSocket, QMutex> &pair);
};

#endif // SERVER_H
