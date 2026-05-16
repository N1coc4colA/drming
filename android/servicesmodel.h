#ifndef SERVICESMODEL_H
#define SERVICESMODEL_H

#include <QAbstractListModel>
#include <QVariantMap>

#include "mdnsmanager.h"

class ServicesModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged)

public:
    enum ServiceRoles { NameRole = Qt::UserRole + 1, HostRole, IpRole, PortRole, TypeRole };

    explicit ServicesModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    Q_INVOKABLE int count() const { return m_services.count(); }
    Q_INVOKABLE QVariantMap get(int index) const;

    Q_INVOKABLE void clear();

Q_SIGNALS:
    void countChanged();

private Q_SLOTS:
    void onServiceFound(const QString &key, const ServiceInfo &info);
    void onServiceLost(const QString &key);
    void onServiceResolved(const QString &key, const ServiceInfo &info);

private:
    QList<QPair<QString, ServiceInfo>> m_services{};

    int findServiceIndex(const QString &name) const;
};

#endif // SERVICESMODEL_H
