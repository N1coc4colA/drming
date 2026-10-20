#ifndef MDNS_H
#define MDNS_H

#include <QHash>
#include <QObject>
#include <QString>

/// Description of a service advertised by the server over mDNS.
struct ServiceInfo
{
    using hash_type = QString;

    QString name;
    QString type;
    QString host;
    QString ip;
    QString protocol;
    int port;

    [[nodiscard]] hash_type toHashable() const { return hash_type{name + type + host + ip + protocol + QString::number(port)}; }

    bool operator==(const ServiceInfo &other) const
    {
        return name == other.name && type == other.type && host == other.host && ip == other.ip && protocol == other.protocol && port == other.port;
    }
};

template<>
struct std::hash<ServiceInfo>
{
    std::size_t operator()(const ServiceInfo &s) const noexcept { return qHash(s.toHashable()); }
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

    static Mdns *instance();

    /// Starts asynchronous discovery using the current platform backend.
    Q_INVOKABLE virtual void startDiscovery() = 0;
    /// Stops discovery and releases backend resources.
    Q_INVOKABLE virtual void stopDiscovery() = 0;

    Q_INVOKABLE [[nodiscard]] int count() const { return static_cast<int>(m_services.count()); }

public Q_SLOTS:
    void onServiceFound(const QString &name, const QString &type, const QString &protocol);
    void onServiceLost(const QString &name, const QString &ip, const QString &protocol);
    void onServiceResolved(const QString &name, const QString &host, const QString &ip, int port, const QString &protocol);

Q_SIGNALS:
    void serviceFound(const QString &key, const ServiceInfo &info, const QString &protocol);
    void serviceLost(const QString &key, const QString &protocol);
    void serviceResolved(const QString &key, const ServiceInfo &info, const QString &protocol);
    void countChanged(int count);

protected:
    QHash<QString, ServiceInfo> m_services{};

private:
    static Mdns *m_instance;
};

#endif // MDNS_H
