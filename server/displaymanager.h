#ifndef DISPLAYMANAGER_H
#define DISPLAYMANAGER_H

#include <QMap>
#include <QMutex>
#include <QObject>
#include <QQueue>
#include <QSet>

class NetworkClient;
class DisplayThread;

class DisplayTable
{
public:
    inline auto contains(auto v) const
    {
        QMutexLocker lock(&m_mtx);
        return containsUnsafe(v);
    }
    inline auto containsUnsafe(auto v) const { return m_displays.contains(v); }

    inline auto insert(auto k, auto v)
    {
        QMutexLocker lock(&m_mtx);
        return insertUnsafe(k, v);
    }
    inline auto insertUnsafe(auto k, auto v) { return m_displays.insert(k, v); }

    inline void apply(auto fn)
    {
        QMutexLocker lock(&m_mtx);
        applyUnsafe(fn);
    }
    inline void applyUnsafe(auto fn)
    {
        for (auto it = m_displays.begin(); it != m_displays.end();) {
            fn(it);
        }
    }

    auto erase(auto v)
    {
        QMutexLocker lock(&m_mtx);
        return eraseUnsafe(v);
    }
    auto eraseUnsafe(auto v) { return m_displays.erase(v); }

private:
    QMap<QByteArray, DisplayThread *> m_displays{};
    mutable QMutex m_mtx;
};

class DisplayManager : public QObject
{
    Q_OBJECT

public:
    explicit DisplayManager(QObject *parent = nullptr);
    ~DisplayManager() override;

    bool registerClient(NetworkClient *client);

    inline DisplayTable *table() { return &m_usedDisplays; }

Q_SIGNALS:
    void noMoreClients();
    void firstClientConnected();

private:
    QQueue<DisplayThread *> m_freeDisplays{};
    DisplayTable m_usedDisplays{};
    int m_count = 0;
};

#endif // DISPLAYMANAGER_H
