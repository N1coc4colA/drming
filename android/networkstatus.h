#ifndef NETWORKSTATE_H
#define NETWORKSTATE_H

#include <QObject>

#ifdef Q_OS_ANDROID
#include <QJniObject>
#endif

class NetworkState : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool connected READ getConnected NOTIFY connectivityChanged)

public:
    explicit NetworkState(QObject *parent = nullptr);

    inline bool getConnected() const { return m_connected; }

Q_SIGNALS:
    void connectivityChanged(bool connected);

public Q_SLOTS:
    inline void onConnectivityChanged(bool connected)
    {
        m_connected = connected;
        Q_EMIT connectivityChanged(connected);
    }

private:
#ifdef Q_OS_ANDROID
    QJniObject m_javaHelper{};
#endif

    bool m_connected = false;
};

#endif // NETWORKSTATE_H
