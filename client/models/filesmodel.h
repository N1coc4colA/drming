#ifndef FILESMODEL_H
#define FILESMODEL_H

#include <QAbstractListModel>
#include <QDateTime>
#include <QVariantMap>

class FilesModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged)

public:
    using MapType = QList<std::tuple<QDateTime, QString, QVariantMap>>;

    enum FileRoles { NameRole = Qt::UserRole + 1, DateTimeRole, InfoRole };

    explicit FilesModel(QObject *parent = nullptr);

    [[nodiscard]] int rowCount(const QModelIndex &parent) const override;
    [[nodiscard]] QVariant data(const QModelIndex &index, int role) const override;
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

    Q_INVOKABLE [[nodiscard]] int count() const { return static_cast<int>(m_files.count()); }
    Q_INVOKABLE [[nodiscard]] QVariantMap get(int index) const;

    void setData(const MapType &newData);

    [[nodiscard]] const MapType &internalData() const { return m_files; }

Q_SIGNALS:
    void countChanged();

private:
    MapType m_files{};
};

#endif // FILESMODEL_H
