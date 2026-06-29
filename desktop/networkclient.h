#ifndef NETWORKCLIENT_H
#define NETWORKCLIENT_H

#include <QHostAddress>

class QUdpSocket;
class QDtls;

class NetworkClient : public QObject
{
    Q_OBJECT

public:
    NetworkClient(const QHostAddress &addr, quint16 port, QDtls *dtls, QUdpSocket *socket, QObject *parent = nullptr);
    ~NetworkClient() = default;

    qint64 write(const QByteArray &data);

    inline QAbstractSocket::SocketState state() const { return QAbstractSocket::ConnectedState; }
    inline QHostAddress peerAddress() const { return m_addr; }
    inline quint16 peerPort() const { return m_port; }
    inline QDtls *dtls() { return m_dtls; }

Q_SIGNALS:
    void disconnected();

private:
    QHostAddress m_addr{};
    quint16 m_port = 0;
    QDtls *m_dtls = nullptr;
    QUdpSocket *m_socket = nullptr;
};

#endif // NETWORKCLIENT_H
