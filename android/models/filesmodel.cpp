#include "filesmodel.h"

FilesModel::FilesModel(QObject *parent)
    : QAbstractListModel(parent)
{}

void FilesModel::setData(const QList<QPair<QDateTime, QString>> &newData)
{
    const int c = count();
    if (!c) {
        return;
    }

    beginRemoveRows({}, 0, c);
    m_files.clear();
    endRemoveRows();

    beginInsertRows({}, 0, newData.length());
    m_files = newData;
    endInsertRows();

    Q_EMIT countChanged();
}

int FilesModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) {
        return 0;
    }

    return m_files.count();
}

QVariant FilesModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_files.count()) {
        return {};
    }

    const auto &v = m_files[index.row()];

    switch (role) {
    case NameRole:
        return v.first;
    case DateTimeRole:
        return v.second;
    default:
        return {};
    }
}

QHash<int, QByteArray> FilesModel::roleNames() const
{
    return {{NameRole, "name"}, {DateTimeRole, "datetime"}};
}

QVariantMap FilesModel::get(int index) const
{
    if (index < 0 || index >= m_files.count()) {
        return {};
    }

    const auto &v = m_files[index];
    return {{"name", v.first}, {"datetime", v.second}};
}
