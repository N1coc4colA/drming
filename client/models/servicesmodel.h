#ifndef SERVICESMODEL_H
#define SERVICESMODEL_H

#include <QAbstractListModel>
#include <QVariantMap>

#include "../mdns.h"

/**
 * Adapts mDNS discovery signals into a list model suitable for QML views.
 *
 * Entries are keyed by the discovery backend's service key so repeated
 * resolve events update an existing row instead of creating duplicates.
 */
class ServicesModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged)

public:
    /// Roles exposed by each discovered service row.
    enum ServiceRoles {
        /// Advertised service name.
        NameRole = Qt::UserRole + 1,
        /// Resolved host name.
        HostRole,
        /// Resolved IP address.
        IpRole,
        /// Resolved service port.
        PortRole,
        /// Advertised service type.
        TypeRole
    };

    explicit ServicesModel(QObject *parent = nullptr);

    /// Supplies the number of discovered rows available to a view.
    [[nodiscard]] int rowCount(const QModelIndex &parent) const override;
    /// Maps a service row and role to its QML value.
    [[nodiscard]] QVariant data(const QModelIndex &index, int role) const override;
    /// Names the endpoint fields consumed by QML.
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

    /// Reports how many services are currently displayed.
    Q_INVOKABLE [[nodiscard]] int count() const
    {
        return static_cast<int>(m_services.count());
    }
    /// Returns one service row as a QML-friendly map.
    Q_INVOKABLE [[nodiscard]] QVariantMap get(int index) const;

    /// Drops stale discovery results, typically after disconnecting.
    Q_INVOKABLE void clear();

Q_SIGNALS:
    /// Emitted after the number of visible services changes.
    void countChanged();

private Q_SLOTS:
    /// Adds or updates a service advertised by mDNS.
    void onServiceFound(const QString &key, const ServiceInfo &info);
    /// Removes a service that is no longer advertised.
    void onServiceLost(const QString &key);
    /// Adds or updates resolved service details.
    void onServiceResolved(const QString &key, const ServiceInfo &info);

private:
    /// Display-order entries paired with discovery keys.
    QList<QPair<QString, ServiceInfo>> m_services{};

    /// Finds a row by discovery key or service name.
    [[nodiscard]] int findServiceIndex(const QString &key) const;
};

#endif // SERVICESMODEL_H
