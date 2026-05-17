#ifndef DISPLAYMANAGER_H
#define DISPLAYMANAGER_H

#include <QObject>
#include <QQueue>
#include <QSet>

class QTcpSocket;
class Display;

class DisplayManager : public QObject
{
public:
    explicit DisplayManager(QObject *parent = nullptr);
    ~DisplayManager();

    void registerClient(QTcpSocket *client);

private:
    QQueue<Display *> m_freeDisplays{};
    QSet<Display *> m_usedDisplays{};
};

#endif // DISPLAYMANAGER_H
