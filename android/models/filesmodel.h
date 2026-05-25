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
    enum FileRoles { NameRole = Qt::UserRole + 1, DateTimeRole };

    explicit FilesModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    Q_INVOKABLE int count() const { return m_files.count(); }
    Q_INVOKABLE QVariantMap get(int index) const;

    void setData(const QList<QPair<QDateTime, QString>> &newData);

Q_SIGNALS:
    void countChanged();

private:
    QList<QPair<QDateTime, QString>> m_files{};
};

#endif // FILESMODEL_H
