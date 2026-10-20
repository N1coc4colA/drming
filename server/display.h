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
    explicit Display(const QString &connectorName, StreamSocket &ss, QObject *parent = nullptr);

    StreamSocket &streamSocket() { return m_ss; }

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

    std::optional<DrmFormat::FormatDescriptor> m_cursorFrameDescriptor{};
    std::optional<DrmFormat::FormatDescriptor> m_vkmsFrameDescriptor{};

    QTimer m_timer{};

    virtual void processImage(const QImage &img) = 0;

private Q_SLOTS:
    void forward();
    void onConnected();
    void onDisconnected();
    void disconnectAllClients();
};

Display *generateNewDisplay(const QString &connectorName, StreamSocket &ss, QObject *parent = nullptr);

#endif // DISPLAY_H
