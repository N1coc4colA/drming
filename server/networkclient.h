#ifndef NETWORKCLIENT_H
#define NETWORKCLIENT_H

#include <QHostAddress>

#include <chrono>

#include "../net/parser.h"
#include "../net/udp.h"
#include "../settings.h"

class QUdpSocket;
class QSslSocket;
class QDtls;
class QTimer;
class Display;

/**
 * Represents one authenticated client and its encrypted UDP session.
 *
 * NetworkClient translates packet-parser callbacks into server actions and
 * drops idle peers when their heartbeat deadline expires.
 */
class NetworkClient : public QObject, public Packets::Parser<NetworkClient>
{
    Q_OBJECT

    using TimePoint = std::chrono::time_point<std::chrono::steady_clock>;
    using Duration = std::chrono::steady_clock::duration;

    static constexpr Duration systemTimeout = std::chrono::duration_cast<Duration>(
        std::chrono::duration<double>(Settings::inactivityTimeout / 1000.0)
    );

public:
    explicit NetworkClient(
        QDtls *dtls,
        const QHostAddress &address,
        quint16 port,
        QPair<QUdpSocket, QMutex> &sharedSocket,
        QObject *parent = nullptr
    );
    ~NetworkClient();

    /// Sends an encrypted datagram through the shared server socket.
    std::optional<qint64> write(const QByteArray &data);
    /// Closes the DTLS session and disconnects the client.
    void close();

    /// Returns the state of the underlying UDP socket.
    [[nodiscard]] QAbstractSocket::SocketState state() const;
    /// Returns the remote peer IP address.
    [[nodiscard]] QHostAddress peerAddress() const
    {
        return m_address;
    }
    /// Returns the remote peer port.
    [[nodiscard]] quint16 peerPort() const
    {
        return m_port;
    }
    /// Returns the SHA-256 digest of the peer certificate.
    [[nodiscard]] QByteArray digest() const;

    // Packet processing stubs (only HeartBeat and Reinit are handled)
    void processPacket(const Packets::HeartBeat &);
    void processPacket(const Packets::Reinit &);
    void processPacket(const Packets::RequestClientResolution &);
    void processPacket(const Packets::RequestKey &);
    void processPacket(const Packets::RequestAudioSource &);
    void processPacket(const Packets::RequestVideoSource &);
    void processPacket(const Packets::ClientTelemetry &t);
    void processPacket(const Packets::RequestKeyframe &);
    void onPacketErrors()
    {}

    /// Updates the timestamp of the most recently received heartbeat.
    void notifyHeartBeat();

    /// Associates this client with the virtual display that serves its frames.
    void setDisplay(Display *disp);

    /// Feeds an encrypted datagram received from this peer into DTLS.
    void incomingEncryptedData(const QByteArray &encrypted);

Q_SIGNALS:
    /// Emitted when the client is disconnected.
    void disconnected();

private:
    Display *m_display = nullptr;
    const QHostAddress m_address;
    const quint16 m_port;

    DtlsReaderWriter<NetworkClient> m_rw;

    QTimer *m_timer = nullptr;
    /// Timestamp of the last received client heartbeat.
    TimePoint m_lastHeartBeat{};

    /// Checks whether the client heartbeat has timed out and disconnects if so.
    void checkHeartBeat();
    /// Sends the current session key update to the client.
    void notifyKey();
};

#endif // NETWORKCLIENT_H
