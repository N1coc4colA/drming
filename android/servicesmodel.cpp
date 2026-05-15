#include "servicesmodel.h"

ServicesModel::ServicesModel(QObject *parent)
    : QAbstractListModel(parent)
{
    MdnsManager &manager = MdnsManager::instance();

    connect(&manager, &MdnsManager::serviceFound, this, &ServicesModel::onServiceFound);
    connect(&manager, &MdnsManager::serviceLost, this, &ServicesModel::onServiceLost);
    connect(&manager, &MdnsManager::serviceResolved, this, &ServicesModel::onServiceResolved);
}

void ServicesModel::clear()
{
    const int c = count();
    if (!c) {
        return;
    }

    beginRemoveRows(QModelIndex(), 0, c);
    m_services.clear();
    endRemoveRows();

    Q_EMIT countChanged();
}

int ServicesModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) {
        return 0;
    }

    return m_services.count();
}

QVariant ServicesModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_services.count()) {
        return {};
    }

    const ServiceInfo &s = m_services.at(index.row());

    switch (role) {
    case NameRole: return s.name;
    case HostRole: return s.host;
    case IpRole:
        return s.ip;
    case PortRole: return s.port;
    case TypeRole: return s.type;
    default:
        return {};
    }
}

QHash<int, QByteArray> ServicesModel::roleNames() const
{
    return {{NameRole, "name"}, {HostRole, "host"}, {IpRole, "ip"}, {PortRole, "port"}, {TypeRole, "type"}};
}

QVariantMap ServicesModel::get(int index) const
{
    if (index < 0 || index >= m_services.count()) {
        return {};
    }

    const ServiceInfo &s = m_services.at(index);
    return {{"name", s.name}, {"host", s.host}, {"ip", s.ip}, {"port", s.port}, {"type", s.type}};
}

int ServicesModel::findServiceIndex(const QString &name) const
{
    for (int i = 0; i < m_services.size(); ++i) {
        if (m_services.at(i).name == name) {
            return i;
        }
    }

    return -1;
}

void ServicesModel::onServiceFound(const ServiceInfo &info)
{
    // If already known, update existing entry (avoid duplicates)
    const int idx = findServiceIndex(info.name);
    if (idx != -1) {
        m_services[idx].type = info.type;

        Q_EMIT dataChanged(index(idx), index(idx));
        return;
    }

    beginInsertRows(QModelIndex(), m_services.count(), m_services.count());
    m_services.append(info);
    endInsertRows();

    Q_EMIT countChanged();
}

void ServicesModel::onServiceLost(const QString &name)
{
    const int idx = findServiceIndex(name);
    if (idx == -1) {
        return;
    }

    beginRemoveRows(QModelIndex(), idx, idx);
    m_services.removeAt(idx);
    endRemoveRows();

    Q_EMIT countChanged();
}

void ServicesModel::onServiceResolved(const ServiceInfo &info)
{
    const int idx = findServiceIndex(info.name);
    if (idx == -1) {
        // If we didn't have the service yet, insert it
        beginInsertRows(QModelIndex(), m_services.count(), m_services.count());
        m_services.append(info);
        endInsertRows();

        Q_EMIT countChanged();
        return;
    }

    m_services[idx] = info;

    Q_EMIT dataChanged(index(idx), index(idx));
}
