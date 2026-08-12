#ifndef NETWORKLINK_H
#define NETWORKLINK_H

#include <optional>
#include <vector>

#include <QDtls>
#include <QImage>
#include <QSslSocket>
#include <QTimer>
#include <QUdpSocket>

#include "../parser.h"

class NetworkLink : public QObject
{
    Q_OBJECT

protected:
    explicit NetworkLink(QObject *parent = nullptr);

public:
    ~NetworkLink() override;

    static NetworkLink *createForPlatform(QObject *parent = nullptr);

Q_SIGNALS:
    void error(const QString &explanation);
    void opened();
    void closed();
    void connectionInitialised();

public Q_SLOTS:
    void close();
    void connect(const QString &address, int port, const QString &clientName, const QString &protocolName);
    virtual void setItem(QObject *item) = 0;

protected:
    void write(const QByteArray &data);

private Q_SLOTS:
    void onConnected();
    void onDtlsDisconnected();
    void onSslDisconnected();
    void onDtlsDataAvailable();
    void onSslDataAvailable();
    void onConnectionTimeout();
    void onError(QAbstractSocket::SocketError error);
    void onSslError(const QSslError &error);

private:
    QString m_protocol;
    QByteArray m_buffer{};
    QSslSocket *m_sslSocket = nullptr;
    QUdpSocket *m_udpSocket = nullptr;
    QDtls *m_dtls = nullptr;
    Packets::JitterBuffer<> m_jitterBuffer;
    Packets::Timestamp m_ts = 0;
    QTimer m_inactivityTimer{};
    quint16 m_format = 0;
    quint32 m_width = 0;
    quint32 m_height = 0;
    quint32 m_stride = 0;
    qsizetype m_imageSize = 0;

    bool m_connectionReady = false;

    void connectDtls(const QSslConfiguration &sslConf, const QHostAddress &hostAddress, int port);
    void connectSsl(const QSslConfiguration &sslConf, const QHostAddress &hostAddress, int port);
    qint64 writeDtls(const QByteArray &data);
    qint64 writeSsl(const QByteArray &data);

    virtual void addData(QByteArray additional) = 0;
};

#endif // NETWORKLINK_H
