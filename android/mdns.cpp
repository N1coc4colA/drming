#include "mdns.h"

#include <QCoreApplication>
#include <QDebug>

#ifdef Q_OS_ANDROID
#include "platform/android/mdns.h"
#else
#include "platform/linux/mdns.h"
#endif

Mdns *Mdns::m_instance = nullptr;

Mdns *Mdns::instance()
{
    if (!m_instance) {
        m_instance = new Platform::Mdns(qApp);
    }

    return m_instance;
}

Mdns::Mdns(QObject *parent)
    : QObject(parent)
{
    assert(!m_instance);
    m_instance = this;
}

void Mdns::onServiceFound(const QString &name, const QString &type)
{
    qDebug() << "Found:" << name << type;

    const auto fullName = name + "*";
    const ServiceInfo info{name, type, "", "", -1};
    m_services[fullName] = info;

    Q_EMIT serviceFound(fullName, info);
    Q_EMIT countChanged(count());
}

void Mdns::onServiceLost(const QString &name, const QString &ip)
{
    qDebug() << "Lost:" << name;

    const auto fullName = name + "*" + ip;
    m_services.remove(fullName);
    if (ip.isEmpty()) {
        QStringList keys{};
        for (const auto &key : m_services.keys()) {
            if (key.startsWith(name)) {
                keys.append(key);
            }
        }

        for (const auto &key : keys) {
            m_services.remove(key);
            Q_EMIT serviceLost(key);
        }
    }

    Q_EMIT serviceLost(fullName);
    Q_EMIT countChanged(count());
}

void Mdns::onServiceResolved(const QString &name, const QString &host, const QString &ip, const int port)
{
    qDebug() << "Resolved:" << name << host << ip << port;

    const auto fullName = name + "*" + ip;
    m_services[fullName].name = name;
    m_services[fullName].host = host;
    m_services[fullName].ip = ip;
    m_services[fullName].port = port;

    Q_EMIT serviceResolved(fullName, m_services[fullName]);
}
