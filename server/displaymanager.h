#ifndef DISPLAYMANAGER_H
#define DISPLAYMANAGER_H

#include <QMap>
#include <QObject>
#include <QQueue>
#include <QSet>

class NetworkClient;
class DisplayThread;
class StreamSocket;

class DisplayManager : public QObject
{
public:
    explicit DisplayManager(StreamSocket &ss, QObject *parent = nullptr);
    ~DisplayManager() override;

    bool registerClient(NetworkClient *client);

private:
    DisplayThread *m_thread = nullptr;
    StreamSocket &m_ss;
};

#endif // DISPLAYMANAGER_H
