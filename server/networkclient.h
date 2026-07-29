#ifndef NETWORKCLIENT_H
#define NETWORKCLIENT_H

#include <QSslSocket>
#include <QUdpSocket>

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
    NetworkClient(QAbstractSocket *socket, QObject *parent);
    ~NetworkClient() override = default;

    virtual qint64 write(const QByteArray &data) = 0;

    [[nodiscard]] QAbstractSocket::SocketState state() const { return QAbstractSocket::ConnectedState; }
    [[nodiscard]] virtual QHostAddress peerAddress() const = 0;
    [[nodiscard]] virtual quint16 peerPort() const = 0;
    [[nodiscard]] virtual QByteArray digest() const = 0;

    inline void close() { m_socket->close(); }

    inline QAbstractSocket *socket() { return m_socket; }
    inline const QAbstractSocket *socket() const { return m_socket; }

    inline void processPacket(const Packets::ServerImage &) {};
    inline void processPacket(const Packets::ClientResolution &) {}
    inline void processPacket(const Packets::ServerBrightness &) {};
    inline void processPacket(const Packets::ServerStream &) {};
    void processPacket(const Packets::HeartBeat &);
    void processPacket(const Packets::Reinit &);
    inline void onPacketErrors() {};

    inline virtual void notifyHeartBeat() {};

    inline void setDisplay(Display *disp) { m_display = disp; };

Q_SIGNALS:
    void disconnected();

protected:
    QAbstractSocket *m_socket = nullptr;

    virtual void readData();

private:
    Display *m_display = nullptr;
};

class NetworkClientDtls : public NetworkClient
{
    using TimePoint = std::chrono::time_point<std::chrono::system_clock>;
    using Duration = std::chrono::system_clock::duration;

    static constexpr Duration systemTimeout = std::chrono::duration_cast<Duration>(
        std::chrono::duration<double>(Settings::inactivityTimeout / 1000.0));

public:
    NetworkClientDtls(QDtls *dtls, QUdpSocket *socket, const QHostAddress &address, quint16 port, QObject *parent = nullptr);
    QDtls *dtls() { return m_dtls; }

    [[nodiscard]] inline QHostAddress peerAddress() const override { return m_address; }
    [[nodiscard]] inline quint16 peerPort() const override { return m_port; }

    qint64 write(const QByteArray &data) override;
    QByteArray digest() const override;

    void notifyHeartBeat() override;

protected:
    void readData() override;

private:
    const QHostAddress m_address;
    const quint16 m_port;

    QDtls *m_dtls = nullptr;
    QTimer *m_timer = nullptr;
    TimePoint m_lastHeartBeat{};

    void checkHeartBeat();
};

class NetworkClientSsl : public NetworkClient
{
public:
    NetworkClientSsl(QSslSocket *socket, QObject *parent = nullptr);

    [[nodiscard]] inline QHostAddress peerAddress() const override { return m_socket->peerAddress(); }
    [[nodiscard]] inline quint16 peerPort() const override { return m_socket->peerPort(); }

    qint64 write(const QByteArray &data) override;
    QByteArray digest() const override;
};

#endif // NETWORKCLIENT_H
