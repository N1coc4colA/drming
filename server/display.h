#ifndef DISPLAY_H
#define DISPLAY_H

#include <QTimer>

#include "displayreader.h"
#include "networkclient.h"

class Display : public QObject
{
    Q_OBJECT

public:
    explicit Display(const QString &connectorName, QObject *parent = nullptr);

Q_SIGNALS:
    void nowFree();

public Q_SLOTS:
    void addClient(NetworkClient *client);
    void reinit();
    void requireResolutionInformation();

protected:
    void sendData(QByteArray output);

private:
    QSet<NetworkClient *> m_clients{};
    QSize m_prevSize{};
    bool primaryFailureNotice = false;

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

Display *generateNewDisplay(const QString &connectorName, QObject *parent = nullptr);

#endif // DISPLAY_H
