#ifndef NETWORKLINK_H
#define NETWORKLINK_H

#include <QImage>
#include <QObject>
#include <QTcpSocket>

class NetworkLink : public QObject
{
    Q_OBJECT

public:
    explicit NetworkLink(QObject *parent = nullptr);
    ~NetworkLink();

Q_SIGNALS:
    void error(const QString &explanation);
    void opened();
    void closed();
    void imageReady(QImage);

public Q_SLOTS:
    void close();
    void connect(const QString &address, int port);

private Q_SLOTS:
    void onConnected();
    void onDisconnected();
    void onError(QAbstractSocket::SocketError err);
    void onDataAvailable();

private:
    QByteArray m_buffer{};
    QTcpSocket *m_socket = nullptr;
    quint16 m_format = 0;
    quint32 m_width = 0;
    quint32 m_height = 0;
    quint32 m_stride = 0;
    qsizetype m_imageSize = 0;

    bool m_connectionReady = false;

    void protocolStateUpdate();
};

#endif // NETWORKLINK_H
