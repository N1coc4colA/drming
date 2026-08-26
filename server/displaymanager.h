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
public:
    explicit DisplayManager(QObject *parent = nullptr);
    ~DisplayManager() override;

    bool registerClient(NetworkClient *client);

private:
    DisplayThread *m_thread = nullptr;
};

#endif // DISPLAYMANAGER_H
