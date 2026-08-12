#ifndef NETWORKCLIENT_H
#define NETWORKCLIENT_H

#include <QSslSocket>
#include <QUdpSocket>
#include <QHostAddress>
#include <chrono>

#include "../parser.h"
#include "../settings.h"

class QUdpSocket;
class QSslSocket;
class QDtls;
class QTimer;
class Display;

class NetworkClient : public QObject, public Packets::Parser<NetworkClient>
{
    Q_OBJECT

public:
    explicit NetworkClient(QObject *parent = nullptr) : QObject(parent), Packets::Parser<NetworkClient>(*this) {}
    ~NetworkClient() override = default;

    virtual QAbstractSocket *socket() = 0;
    virtual qint64 write(const QByteArray &data) = 0;
    virtual void close() = 0;

    [[nodiscard]] virtual QAbstractSocket::SocketState state() const = 0;
    [[nodiscard]] virtual QHostAddress peerAddress() const = 0;
    [[nodiscard]] virtual quint16 peerPort() const = 0;
    [[nodiscard]] virtual QByteArray digest() const = 0;

    // Packet processing stubs (only HeartBeat and Reinit are handled)
    void processPacket(const Packets::HeartBeat &);
    void processPacket(const Packets::Reinit &);
    void processPacket(const Packets::RequestClientResolution &);
    void onPacketErrors() {}

    virtual void notifyHeartBeat() {}

    void setDisplay(Display *disp) { m_display = disp; }

Q_SIGNALS:
    void disconnected();

protected:
    Display *m_display = nullptr;
};

class NetworkClientDtls : public NetworkClient
{
    Q_OBJECT

    using TimePoint = std::chrono::time_point<std::chrono::system_clock>;
    using Duration = std::chrono::system_clock::duration;

    static constexpr Duration systemTimeout = std::chrono::duration_cast<Duration>(
        std::chrono::duration<double>(Settings::inactivityTimeout / 1000.0));

public:
    // Takes ownership of dtls, uses sharedSocket for sending/receiving
    NetworkClientDtls(QDtls *dtls, const QHostAddress &address, quint16 port,
                      QUdpSocket *sharedSocket, QObject *parent = nullptr);
    ~NetworkClientDtls() override;

    // Feed an encrypted datagram received from this peer
    void incomingEncryptedData(const QByteArray &encrypted);

    // NetworkClient interface
    inline QAbstractSocket *socket() override { return m_sharedSocket; };
    qint64 write(const QByteArray &data) override;
    void close() override;
    QAbstractSocket::SocketState state() const override;
    QHostAddress peerAddress() const override { return m_address; }
    quint16 peerPort() const override { return m_port; }
    QByteArray digest() const override;

    void notifyHeartBeat() override;

private:
    void checkHeartBeat();

    Packets::JitterBuffer<> m_jitterBuffer;
    Packets::Timestamp m_ts = 0;
    QDtls *m_dtls;
    const QHostAddress m_address;
    const quint16 m_port;
    QUdpSocket *m_sharedSocket;
    QTimer *m_timer;
    TimePoint m_lastHeartBeat;
};

class NetworkClientSsl : public NetworkClient
{
    Q_OBJECT

public:
    explicit NetworkClientSsl(QSslSocket *socket, QObject *parent = nullptr);

    inline QAbstractSocket *socket() override { return m_socket; };
    qint64 write(const QByteArray &data) override;
    void close() override { m_socket->close(); }
    QAbstractSocket::SocketState state() const override { return m_socket->state(); }
    QHostAddress peerAddress() const override { return m_socket->peerAddress(); }
    quint16 peerPort() const override { return m_socket->peerPort(); }
    QByteArray digest() const override;

protected:
    QSslSocket *m_socket;
};

#endif // NETWORKCLIENT_H
