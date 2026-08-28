#ifndef NETWORKCLIENT_H
#define NETWORKCLIENT_H

#include <QHostAddress>
#include <QMutex>
#include <QSslSocket>
#include <QUdpSocket>

#include <chrono>

#include "../net/parser.h"
#include "../settings.h"

class QUdpSocket;
class QSslSocket;
class QDtls;
class QTimer;
class Display;

class NetworkClient : public QObject, public Packets::Parser<NetworkClient>
{
    Q_OBJECT

    using TimePoint = std::chrono::time_point<std::chrono::system_clock>;
    using Duration = std::chrono::system_clock::duration;

    static constexpr Duration systemTimeout = std::chrono::duration_cast<Duration>(
        std::chrono::duration<double>(Settings::inactivityTimeout / 1000.0));

public:
    explicit NetworkClient(QDtls *dtls, const QHostAddress &address, quint16 port, QPair<QUdpSocket, QMutex> &sharedSocket, QObject *parent = nullptr);
    ~NetworkClient();

    inline QAbstractSocket *socket() { return &m_sharedSocket.first; };
    qint64 write(const QByteArray &data);
    void close();

    [[nodiscard]] QAbstractSocket::SocketState state() const;
    [[nodiscard]] QHostAddress peerAddress() const { return m_address; }
    [[nodiscard]] quint16 peerPort() const { return m_port; }
    [[nodiscard]] QByteArray digest() const;

    // Packet processing stubs (only HeartBeat and Reinit are handled)
    void processPacket(const Packets::HeartBeat &);
    void processPacket(const Packets::Reinit &);
    void processPacket(const Packets::RequestClientResolution &);
    void processPacket(const Packets::RequestKey &);
    void onPacketErrors() {}

    void notifyHeartBeat();

    void setDisplay(Display *disp) { m_display = disp; }

    // Feed an encrypted datagram received from this peer
    void incomingEncryptedData(const QByteArray &encrypted);

Q_SIGNALS:
    void disconnected();

private:
    Display *m_display = nullptr;

    Packets::JitterBuffer<> m_jitterBuffer;
    Packets::Timestamp m_ts = 0;
    QDtls *m_dtls;
    const QHostAddress m_address;
    const quint16 m_port;
    QPair<QUdpSocket, QMutex> &m_sharedSocket;
    QTimer *m_timer;
    TimePoint m_lastHeartBeat;

    void checkHeartBeat();
};

#endif // NETWORKCLIENT_H
