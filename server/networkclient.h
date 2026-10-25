#ifndef NETWORKCLIENT_H
#define NETWORKCLIENT_H

#include <QHostAddress>
#include <QMutex>
#include <QSslSocket>
#include <QUdpSocket>

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

    using TimePoint = std::chrono::time_point<std::chrono::system_clock>;
    using Duration = std::chrono::system_clock::duration;

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
    qint64 write(const QByteArray &data);
    void close();

    [[nodiscard]] QAbstractSocket::SocketState state() const;
    [[nodiscard]] QHostAddress peerAddress() const
    {
        return m_address;
    }
    [[nodiscard]] quint16 peerPort() const
    {
        return m_port;
    }
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

    void notifyHeartBeat();

    /// Associates this client with the virtual display that serves its frames.
    void setDisplay(Display *disp);

    /// Feeds an encrypted datagram received from this peer into DTLS.
    void incomingEncryptedData(const QByteArray &encrypted);

Q_SIGNALS:
    void disconnected();

private:
    Display *m_display = nullptr;
    const QHostAddress m_address;
    const quint16 m_port;

    DtlsReaderWriter<NetworkClient> m_rw;

    QTimer *m_timer;
    TimePoint m_lastHeartBeat;

    void checkHeartBeat();
    void notifyKey();
};

#endif // NETWORKCLIENT_H
