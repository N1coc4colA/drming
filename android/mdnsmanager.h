#ifndef MDNSMANAGER_H
#define MDNSMANAGER_H

#include <QHash>
#include <QObject>
#include <QString>

#ifdef Q_OS_ANDROID
#include <QJniObject>
#else
class AvahiDiscoverer;
#endif

#include "networkstatus.h"

struct ServiceInfo
{
    using hash_type = QString;

    QString name;
    QString type;
    QString host;
    QString ip;
    int port;

    inline hash_type toHashable() const { return hash_type{name + type + host + ip + QString::number(port)}; }

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

class MdnsManager : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged)

public:
    explicit MdnsManager(QObject *parent = nullptr);
#ifndef Q_OS_ANDROID
    ~MdnsManager();
#endif

    static MdnsManager &instance();

    Q_INVOKABLE void startDiscovery();
    void stopDiscovery();

    Q_INVOKABLE inline int count() const { return m_services.count(); }

    inline NetworkState &networkState() { return m_netState; }

public Q_SLOTS:
    void onServiceFound(const QString &name, const QString &type);
    void onServiceLost(const QString &name);
    void onServiceResolved(const QString &name, const QString &host, const QString &ip, int port);

Q_SIGNALS:
    void serviceFound(const ServiceInfo &info);
    void serviceLost(const QString &name);
    void serviceResolved(const ServiceInfo &info);
    void countChanged(int count);

private:
    NetworkState m_netState{};
    QHash<QString, ServiceInfo> m_services{};

    static MdnsManager *m_instance;

#ifdef Q_OS_ANDROID
    QJniObject m_javaHelper{};
#else
    AvahiDiscoverer *m_avahi = nullptr;

Q_SIGNALS:
    void dispatchServiceFound(const QString &name, const QString &type);
    void dispatchServiceLost(const QString &name);
    void dispatchServiceResolved(const QString &name, const QString &host, const QString &ip, int port);

    friend class AvahiDiscoverer;
#endif
};

#endif // MDNSMANAGER_H
