#ifndef NETWORKLINK_H
#define NETWORKLINK_H

#include <QDtls>
#include <QImage>
#include <QSslSocket>
#include <QTimer>
#include <QUdpSocket>

#include "../net/udp.h"
#include "../net/telemetry.h"

/**
 * Client-side secure transport for control packets and video frames.
 *
 * TLS carries the reliable control channel. DTLS/UDP is used for frame data
 * when the negotiated protocol requests it; platform subclasses consume the
 * decoded image payload through setItem/pushData.
 */
class NetworkLink : public QObject
{
    Q_OBJECT

protected:
    explicit NetworkLink(QObject *parent = nullptr);

public:
    ~NetworkLink() override;

    static NetworkLink *createForPlatform(QObject *parent = nullptr);

    /// Supplies additional bytes to the platform-specific frame decoder.
    virtual void pushData(QByteArray additional) = 0;

Q_SIGNALS:
    void error(const QString &explanation);
    void opened();
    void closed();
    void connectionInitialised();
    void connectionReady();

public Q_SLOTS:
    /// Closes both transport channels and resets negotiated frame state.
    void close();
    /// Starts the TLS/DTLS handshake with a discovered server.
    void connect(const QString &address, int port, const QString &clientName, const QString &protocolName);
    virtual void setItem(QObject *item) = 0;

protected:
    void write(const QByteArray &data);

private Q_SLOTS:
    void onSslConnected();
    void onSslEncrypted();
    void onUdpConnected();

    void onDtlsDisconnected();
    void onSslDisconnected();

    void onDtlsDataAvailable();
    void onSslDataAvailable();

    void onConnectionTimeout();
    void onError(QAbstractSocket::SocketError error);
    void onSslError(const QSslError &error);

    void onTelemetryTimeout();

protected:
    QTimer m_inactivityTimer{};

    Telemetry m_telemetry{};
    QTimer m_telemetryTimer{};
    qint64 m_lastTelemetryNs = 0;

private:
    QString m_protocol;
    QByteArray m_buffer{};
    QSslSocket *m_sslSocket = nullptr;
    QUdpSocket *m_udpSocket = nullptr;
    QDtls *m_dtls = nullptr;

    DtlsReaderWriter<NetworkLink> m_rw;

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
    void resetSessionState();

    friend class DtlsReader<NetworkLink>;
};

#endif // NETWORKLINK_H
