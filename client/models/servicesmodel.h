#ifndef SERVICESMODEL_H
#define SERVICESMODEL_H

#include <QAbstractListModel>
#include <QVariantMap>

#include "../mdns.h"

class ServicesModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged)

public:
    enum ServiceRoles { NameRole = Qt::UserRole + 1, HostRole, IpRole, PortRole, TypeRole, ProtocolRole };

    explicit ServicesModel(QObject *parent = nullptr);

    [[nodiscard]] int rowCount(const QModelIndex &parent) const override;
    [[nodiscard]] QVariant data(const QModelIndex &index, int role) const override;
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

    Q_INVOKABLE [[nodiscard]] int count() const { return static_cast<int>(m_services.count()); }
    Q_INVOKABLE [[nodiscard]] QVariantMap get(int index) const;

    Q_INVOKABLE void clear();

Q_SIGNALS:
    void countChanged();

private Q_SLOTS:
    void onServiceFound(const QString &key, const ServiceInfo &info);
    void onServiceLost(const QString &key);
    void onServiceResolved(const QString &key, const ServiceInfo &info);

private:
    QList<QPair<QString, ServiceInfo>> m_services{};

    [[nodiscard]] int findServiceIndex(const QString &key) const;
};

#endif // SERVICESMODEL_H
