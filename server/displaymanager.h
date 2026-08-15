#ifndef DISPLAYMANAGER_H
#define DISPLAYMANAGER_H

#include <QMap>
#include <QObject>
#include <QQueue>
#include <QSet>

class NetworkClient;
class DisplayThread;

class DisplayManager : public QObject
{
    Q_OBJECT

public:
    explicit DisplayManager(QObject *parent = nullptr);
    ~DisplayManager() override;

    bool registerClient(NetworkClient *client);

Q_SIGNALS:
    void noMoreClients();
    void firstClientConnected();

private:
    QQueue<DisplayThread *> m_freeDisplays{};
    QMap<QByteArray, DisplayThread *> m_usedDisplays{};
    int m_count = 0;
};

#endif // DISPLAYMANAGER_H
