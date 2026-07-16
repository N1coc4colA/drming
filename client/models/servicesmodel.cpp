#include "servicesmodel.h"

ServicesModel::ServicesModel(QObject *parent)
    : QAbstractListModel(parent)
{
    const auto mdnsInst = Mdns::instance();

    connect(mdnsInst, &Mdns::serviceFound, this, &ServicesModel::onServiceFound);
    connect(mdnsInst, &Mdns::serviceLost, this, &ServicesModel::onServiceLost);
    connect(mdnsInst, &Mdns::serviceResolved, this, &ServicesModel::onServiceResolved);
}

void ServicesModel::clear()
{
    const auto c = count();
    if (!c) {
        return;
    }

    beginRemoveRows(QModelIndex(), 0, c - 1);
    m_services.clear();
    endRemoveRows();

    Q_EMIT countChanged();
}

int ServicesModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(m_services.count());
}

QVariant ServicesModel::data(const QModelIndex &index, const int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_services.count()) {
        return {};
    }

    const ServiceInfo &s = m_services.at(index.row()).second;

    switch (role) {
    case NameRole:
        return s.name;
    case HostRole:
        return s.host;
    case IpRole:
        return s.ip;
    case PortRole:
        return s.port;
    case TypeRole:
        return s.type;
    case ProtocolRole:
        return s.protocol;
    default:
        return {};
    }
}

QHash<int, QByteArray> ServicesModel::roleNames() const
{
    return {{NameRole, "name"}, {HostRole, "host"}, {IpRole, "ip"}, {PortRole, "port"}, {TypeRole, "type"}, {ProtocolRole, "protocol"}};
}

QVariantMap ServicesModel::get(const int index) const
{
    if (index < 0 || index >= m_services.count()) {
        return {};
    }

    const ServiceInfo &s = m_services.at(index).second;
    return {{"name", s.name}, {"host", s.host}, {"ip", s.ip}, {"port", s.port}, {"type", s.type}};
}

int ServicesModel::findServiceIndex(const QString &key) const
{
    for (int i = 0; i < m_services.size(); ++i) {
        if (m_services.at(i).first == key) {
            return i;
        }
    }

    return -1;
}

void ServicesModel::onServiceFound(const QString &key, const ServiceInfo &info)
{
    Q_UNUSED(key);

    // If already known, update existing entry (avoid duplicates)
    const auto idx = findServiceIndex(info.name);
    if (idx != -1) {
        m_services[idx].second.type = info.type;

        Q_EMIT dataChanged(index(idx), index(idx));
        return;
    }

    // [TODO] See for cleanup
    /*beginInsertRows(QModelIndex(), m_services.count(), m_services.count());
    m_services.append({key, info});
    endInsertRows();

    Q_EMIT countChanged();*/
}

void ServicesModel::onServiceLost(const QString &key)
{
    const auto idx = findServiceIndex(key);
    if (idx == -1) {
        return;
    }

    beginRemoveRows(QModelIndex(), idx, idx);
    m_services.removeAt(idx);
    endRemoveRows();

    Q_EMIT countChanged();
}

void ServicesModel::onServiceResolved(const QString &key, const ServiceInfo &info)
{
    const auto idx = findServiceIndex(info.name);
    if (idx == -1) {
        // If we didn't have the service yet, insert it
        const auto servicesCount = static_cast<int>(m_services.count());
        beginInsertRows(QModelIndex(), servicesCount, servicesCount);
        m_services.append({key, info});
        endInsertRows();

        Q_EMIT countChanged();
        return;
    }

    m_services[idx] = {key, info};

    Q_EMIT dataChanged(index(idx), index(idx));
}
