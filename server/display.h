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
    void nowFree(Display *);

public Q_SLOTS:
    void setClient(NetworkClient *client);

protected:
    void sendData(QByteArray output);

private:
    NetworkClient *m_client = nullptr;
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
};

Display *generateNewDisplay(const QString &connectorName, QObject *parent = nullptr);

#endif // DISPLAY_H
