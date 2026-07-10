#ifndef MDNS_H
#define MDNS_H

#include <QHash>
#include <QObject>
#include <QString>

struct ServiceInfo
{
    using hash_type = QString;

    QString name;
    QString type;
    QString host;
    QString ip;
    int port;

    [[nodiscard]] hash_type toHashable() const { return hash_type{name + type + host + ip + QString::number(port)}; }

    bool operator==(const ServiceInfo &other) const
    {
        return name == other.name && type == other.type && host == other.host && ip == other.ip && port == other.port;
    }
};

template<>
struct std::hash<ServiceInfo>
{
    std::size_t operator()(const ServiceInfo &s) const noexcept { return qHash(s.toHashable()); }
};

class Mdns : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged)

public:
    explicit Mdns(QObject *parent = nullptr);
    ~Mdns() override = default;

    static Mdns *instance();

    Q_INVOKABLE virtual void startDiscovery() = 0;
    Q_INVOKABLE virtual void stopDiscovery() = 0;

    Q_INVOKABLE [[nodiscard]] int count() const { return static_cast<int>(m_services.count()); }

public Q_SLOTS:
    void onServiceFound(const QString &name, const QString &type);
    void onServiceLost(const QString &name, const QString &ip);
    void onServiceResolved(const QString &name, const QString &host, const QString &ip, int port);

Q_SIGNALS:
    void serviceFound(const QString &key, const ServiceInfo &info);
    void serviceLost(const QString &key);
    void serviceResolved(const QString &key, const ServiceInfo &info);
    void countChanged(int count);

protected:
    QHash<QString, ServiceInfo> m_services{};

private:
    static Mdns *m_instance;
};

#endif // MDNS_H
