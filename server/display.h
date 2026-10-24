#ifndef DISPLAY_H
#define DISPLAY_H

#include <QTimer>

#include "displayreader.h"
#include "networkclient.h"

class StreamSocket;

/**
 * Captures one virtual DRM connector and distributes its frames to clients.
 *
 * Subclasses only provide the output encoding; capture, cursor composition,
 * client membership, and display reinitialization live here.
 */
class Display : public QObject
{
    Q_OBJECT

public:
    explicit Display(const QString &connectorName, StreamSocket &ss, bool inPlaceComposite, QObject *parent = nullptr);

    StreamSocket &streamSocket() { return m_ss; }

    void applyClientTelemetry(const Packets::ClientTelemetry &t);

Q_SIGNALS:
    void nowFree();

public Q_SLOTS:
    /// Adds a client to the display and sends its initial stream metadata.
    void addClient(NetworkClient *client);
    /// Rebuilds capture state after the virtual display changes.
    void reinit();
    /// Requests the client to report the resolution it can display.
    void requireResolutionInformation();

protected:
    void sendData(QByteArray output);

private:
    QSet<NetworkClient *> m_clients{};
    QSize m_prevSize{};
    bool primaryFailureNotice = false;

    StreamSocket &m_ss;
    DisplayReader m_reader;

    /// rate-limiter state
    qint64 m_lastForwardNs = 0;
    /// idle keepalive state
    qint64 m_lastKeepAliveNs = 0;
    uint32_t m_lastFbId = 0;
    QTimer m_pollTimer{};
    QImage m_compositeBuffer{};

    std::optional<DrmFormat::FormatDescriptor> m_cursorFrameDescriptor{};
    std::optional<DrmFormat::FormatDescriptor> m_vkmsFrameDescriptor{};

    std::atomic<int> m_targetFps{Settings::maxForwardFps};
    int m_overloadStreak = 0;
    int m_healthyStreak = 0;
    qint64 m_encodeSumNs = 0;
    int m_encodeSamples = 0;

    virtual void processImage(const QImage &img) = 0;

private Q_SLOTS:
    void forward();
    void onConnected();
    void onDisconnected();
    void disconnectAllClients();
};

Display *generateNewDisplay(const QString &connectorName, StreamSocket &ss, QObject *parent = nullptr);

#endif // DISPLAY_H
