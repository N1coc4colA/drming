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

    /// Returns the stream socket used for video transmission.
    StreamSocket &streamSocket()
    {
        return m_ss;
    }
    const StreamSocket &streamSocket() const
    {
        return m_ss;
    }

    /// Updates dynamic encoding settings based on client telemetry.
    void applyClientTelemetry(const Packets::ClientTelemetry &t);
    /// Forces the encoder to generate an IDR keyframe on the next capture cycle.
    virtual void forceKeyframe();

Q_SIGNALS:
    /// Emitted when all clients have disconnected and the display is idle.
    void nowFree();

public Q_SLOTS:
    /// Adds a client to the display and sends its initial stream metadata.
    void addClient(NetworkClient *client);
    /// Rebuilds capture state after the virtual display changes.
    void reinit();
    /// Requests the client to report the resolution it can display.
    void requireResolutionInformation();

protected:
    /// Transmits encoded frame data to connected clients.
    bool sendData(QByteArray output);

private:
    /// Set of clients receiving this display stream.
    QSet<NetworkClient *> m_clients{};
    /// Dimensions of the previously captured frame.
    QSize m_prevSize{};
    /// Whether a failure to read the primary DRM plane has already been logged.
    bool primaryFailureNotice = false;

    /// Video stream socket instance.
    StreamSocket &m_ss;
    /// Reads framebuffer and cursor data from DRM.
    DisplayReader m_reader;

    /// rate-limiter state
    qint64 m_lastForwardNs = 0;
    /// idle keepalive state
    qint64 m_lastKeepAliveNs = 0;
    /// Framebuffer ID from the previous capture cycle.
    uint32_t m_lastFbId = 0;
    /// Periodic timer driving display capture and forwarding.
    QTimer m_pollTimer{};
    /// Software compositing buffer combining desktop and hardware cursor.
    QImage m_compositeBuffer{};

    std::optional<DrmFormat::FormatDescriptor> m_cursorFrameDescriptor{};
    std::optional<DrmFormat::FormatDescriptor> m_vkmsFrameDescriptor{};

    std::atomic<bool> m_forceNextFrame{ false };
    std::atomic<int> m_targetFps{ Settings::maxForwardFps };
    /// Consecutive measurement windows with client overload.
    int m_overloadStreak = 0;
    /// Consecutive measurement windows with healthy client state.
    int m_healthyStreak = 0;
    /// Cumulative frame encoding time in nanoseconds across samples.
    qint64 m_encodeSumNs = 0;
    /// Number of encode duration samples collected.
    int m_encodeSamples = 0;

    virtual void processImage(const QImage &img) = 0;

private Q_SLOTS:
    /// Captures a display frame, encodes it, and forwards it to clients.
    void forward();
    /// Handles client connection and begins frame capture if needed.
    void onConnected();
    /// Handles client disconnection and stops capture when no clients remain.
    void onDisconnected();
    /// Disconnects all currently attached clients.
    void disconnectAllClients();
};

/// Factory function creating a Display instance for the given connector.
Display *generateNewDisplay(const QString &connectorName, StreamSocket &ss, QObject *parent = nullptr);

#endif // DISPLAY_H
