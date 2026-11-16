#ifndef NETWORKSTATE_H
#define NETWORKSTATE_H

#include <QObject>

/// Exposes the platform network link state as a QML-friendly singleton.
class NetworkState : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool connected READ getConnected NOTIFY connectivityChanged)

public:
    explicit NetworkState(QObject *parent = nullptr);

    /// Returns whether the platform currently reports connectivity.
    [[nodiscard]] bool getConnected() const
    {
        return m_connected;
    }

    /// Returns the process-wide network state object.
    static NetworkState *instance();

Q_SIGNALS:
    /// Emitted when platform connectivity changes.
    void connectivityChanged(bool connected);

public Q_SLOTS:
    /// Updates connectivity and clears stale services when offline.
    void onConnectivityChanged(bool connected);

protected:
    /// Last connectivity state reported by the platform.
    bool m_connected = false;

private:
    /// Process-wide network state object.
    static NetworkState *m_instance;
};

#endif // NETWORKSTATE_H
