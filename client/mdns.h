#ifndef MDNS_H
#define MDNS_H

#include <QHash>
#include <QObject>
#include <QString>

#include "helpers.h"

/// Description of a service advertised by the server over mDNS.
struct ServiceInfo
{
    /// Text type used to build stable hash keys.
    using hash_type = QString;

    /// Advertised service name.
    QString name{};
    /// Advertised service type.
    QString type{};
    /// Resolved host name.
    QString host{};
    /// Resolved IP address.
    QString ip{};
    /// Resolved service port.
    int port = -1;

    /// Builds the stable textual service key.
    [[nodiscard]] hash_type toHashable() const
    {
        return hash_type{ name + type + host + ip + QString::number(port) };
    }

    /// Compares every service field.
    bool operator==(const ServiceInfo &other) const
    {
        return name == other.name && type == other.type && host == other.host && ip == other.ip && port == other.port;
    }
};

template<>
struct std::hash<ServiceInfo>
{
    /// Hashes a service through its stable textual key.
    std::size_t operator()(const ServiceInfo &s) const noexcept
    {
        return qHash(s.toHashable());
    }
};

/**
 * Normalizes platform mDNS callbacks into a Qt signal-based service registry.
 *
 * Discovery backends call the onService* slots; consumers observe the
 * corresponding signals without depending on platform-specific APIs directly.
 */
class Mdns : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged)

public:
    explicit Mdns(QObject *parent = nullptr);
    ~Mdns() override = default;

    /// Returns the process-wide mDNS manager.
    static Mdns *instance();

    /// Starts asynchronous discovery using the current platform backend.
    Q_INVOKABLE virtual void startDiscovery() = 0;
    /// Stops discovery and releases backend resources.
    Q_INVOKABLE virtual void stopDiscovery() = 0;

    /// Returns the number of tracked services.
    Q_INVOKABLE [[nodiscard]] int count() const
    {
        return narrow<int>(m_services.count());
    }

public Q_SLOTS:
    /// Records a newly advertised service.
    void onServiceFound(const QString &name, const QString &type);
    /// Removes a service or all of its addresses.
    void onServiceLost(const QString &name, const QString &ip);
    /// Records resolved host, address, and port details.
    void onServiceResolved(const QString &name, const QString &host, const QString &ip, int port);

Q_SIGNALS:
    /// Emitted when discovery reports a new service.
    void serviceFound(const QString &key, const ServiceInfo &info);
    /// Emitted when a service address disappears.
    void serviceLost(const QString &key);
    /// Emitted when a service has been resolved.
    void serviceResolved(const QString &key, const ServiceInfo &info);
    /// Emitted whenever the registry size changes.
    void countChanged(int count);

protected:
    /// Discovered services, indexed by backend key.
    QHash<QString, ServiceInfo> m_services{};

private:
    /// Process-wide mDNS manager.
    static Mdns *m_instance;
};

#endif // MDNS_H
