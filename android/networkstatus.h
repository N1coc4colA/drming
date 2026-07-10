#ifndef NETWORKSTATE_H
#define NETWORKSTATE_H

#include <QObject>

class NetworkState : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool connected READ getConnected NOTIFY connectivityChanged)

public:
    explicit NetworkState(QObject *parent = nullptr);

    [[nodiscard]] bool getConnected() const { return m_connected; }

    static NetworkState *instance();

Q_SIGNALS:
    void connectivityChanged(bool connected);

public Q_SLOTS:
    void onConnectivityChanged(bool connected);

protected:
    bool m_connected = false;

private:
    static NetworkState *m_instance;
};

#endif // NETWORKSTATE_H
