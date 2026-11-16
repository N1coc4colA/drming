#ifndef NETWORKLINK_H
#define NETWORKLINK_H

#include <QImage>
#include <QTimer>

#include "../net/telemetry.h"
#include "../net/udp.h"

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
    /// Establishes the sockets, timers, and framing state shared by transports.
    explicit NetworkLink(QObject *parent = nullptr);

public:
    /// Closes active transports before destroying the link.
    ~NetworkLink() override;

    /// Selects the transport implementation for the current platform.
    static NetworkLink *createForPlatform(QObject *parent = nullptr);

    /// Supplies additional bytes to the platform-specific frame decoder.
    virtual void pushData(const QByteArray &additional) = 0;

Q_SIGNALS:
    /// Reports a transport error suitable for display.
    void error(const QString &explanation);
    /// Emitted when the underlying socket is opened.
    void opened();
    /// Emitted when the underlying socket is closed.
    void closed();
    /// Emitted once the initial transport is established.
    void connectionInitialised();
    /// Emitted when encrypted communication is ready.
    void connectionReady();

public Q_SLOTS:
    /// Closes both transport channels and resets negotiated frame state.
    void close();
    /// Connects to a server using the named client credentials and starts the TLS/DTLS handshake.
    void connect(const QString &address, int port, const QString &clientName, const QString &interfaceName);
    /// Connects the platform renderer to decoded frame delivery.
    virtual void setItem(QObject *item) = 0;

protected:
    /// Restarts the inactivity timeout after network activity.
    void markActivity();
    /// Writes data over the ready secure transport.
    std::optional<quint64> write(const QByteArray &data);

    /// Lets the platform implementation release resources.
    virtual void closePlatform() {}

private Q_SLOTS:
    /// Completes setup after the UDP socket connects.
    void onUdpConnected();
    /// Marks the session inactive and clears transport buffers after disconnection.
    void onDtlsDisconnected();
    /// Processes pending UDP datagrams.
    void onDtlsDataAvailable();
    /// Reports an inactive encrypted connection.
    void onConnectionTimeout();
    /// Converts socket failures into transport errors.
    void onError(QAbstractSocket::SocketError error);

    /// Periodically sends accumulated transport telemetry.
    void onTelemetryTimeout();

protected:
    /// Detects a connection that has stopped receiving activity.
    QTimer m_inactivityTimer{};

    /// Measures frame and queue behavior for the active connection.
    Telemetry m_telemetry{};
    /// Schedules periodic telemetry reports.
    QTimer m_telemetryTimer{};
    /// Anchors the interval used for the next telemetry snapshot.
    quint64 m_lastTelemetryNs = 0;
    /// Interface used to perform connections.
    QString m_interfaceName = "";

private:
    /// Identifies the protocol selected for the session.
    QString m_protocol{};
    /// Holds control bytes awaiting packet parsing.
    QByteArray m_buffer{};
    /// UDP socket carrying DTLS datagrams.
    QUdpSocket *m_udpSocket = nullptr;
    /// DTLS session layered over the UDP socket.
    QDtls *m_dtls = nullptr;

    /// Reader/writer for framed DTLS packets.
    DtlsReaderWriter<NetworkLink> m_rw;

    /// Pixel format announced by the server.
    quint16 m_format = 0;
    /// Width announced by the server.
    quint32 m_width = 0;
    /// Height announced by the server.
    quint32 m_height = 0;
    /// Row stride announced by the server.
    quint32 m_stride = 0;
    /// Buffer size calculated from the negotiated image layout.
    qsizetype m_imageSize = 0;

    /// Tracks whether session negotiation completed.
    bool m_connectionReady = false;

    /// Starts a DTLS session with the supplied peer configuration.
    void connectDtls(const QSslConfiguration &sslConf, const QHostAddress &hostAddress, int port);
    /// Writes a framed payload through DTLS.
    std::optional<quint64> writeDtls(const QByteArray &data);
    /// Clears all state left by a previous connection.
    void resetSessionState();

    friend class DtlsReader<NetworkLink>;
};

#endif // NETWORKLINK_H
