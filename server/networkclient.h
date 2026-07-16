#ifndef NETWORKCLIENT_H
#define NETWORKCLIENT_H

#include <QSslSocket>
#include <QUdpSocket>

class QUdpSocket;
class QSslSocket;
class QDtls;

class NetworkClient : public QObject
{
    Q_OBJECT

public:
    NetworkClient(QAbstractSocket *socket, QObject *parent);
    ~NetworkClient() override = default;

    virtual qint64 write(const QByteArray &data) = 0;

    [[nodiscard]] QAbstractSocket::SocketState state() const { return QAbstractSocket::ConnectedState; }
    [[nodiscard]] inline QHostAddress peerAddress() const { return m_socket->peerAddress(); }
    [[nodiscard]] inline quint16 peerPort() const { return m_socket->peerPort(); }

    inline QAbstractSocket *socket() { return m_socket; }
    inline const QAbstractSocket *socket() const { return m_socket; }

Q_SIGNALS:
    void disconnected();

protected:
    QAbstractSocket *m_socket = nullptr;
};

class NetworkClientDtls : public NetworkClient
{
public:
    NetworkClientDtls(QDtls *dtls, QUdpSocket *socket, QObject *parent = nullptr);
    QDtls *dtls() { return m_dtls; }

    qint64 write(const QByteArray &data) override;

private:
    QDtls *m_dtls = nullptr;
};

class NetworkClientSsl : public NetworkClient
{
public:
    NetworkClientSsl(QSslSocket *socket, QObject *parent = nullptr);

    qint64 write(const QByteArray &data) override;
};

#endif // NETWORKCLIENT_H
