#ifndef DISPLAYMANAGER_H
#define DISPLAYMANAGER_H

#include <QMap>
#include <QObject>
#include <QQueue>
#include <QSet>

class NetworkClient;
class Display;

class DisplayManager : public QObject
{
public:
    explicit DisplayManager(QObject *parent = nullptr);
    ~DisplayManager() override;

    bool registerClient(NetworkClient *client);

private:
    QQueue<Display *> m_freeDisplays{};
    QMap<QByteArray, Display *> m_usedDisplays{};
};

#endif // DISPLAYMANAGER_H
