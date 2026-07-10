#include "filesmodel.h"

#include "../settings.h"

FilesModel::FilesModel(QObject *parent)
    : QAbstractListModel(parent)
{}

void FilesModel::setData(const MapType &newData)
{
    beginResetModel();
    m_files = newData;
    endResetModel();

    Q_EMIT countChanged();
}

int FilesModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(m_files.count());
}

QVariant FilesModel::data(const QModelIndex &index, const int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_files.count()) {
        return {};
    }

    const auto &v = m_files[index.row()];

    switch (role) {
    case DateTimeRole: {
        const QString localeDateFormat = QLocale::system().dateFormat(QLocale::ShortFormat);
        return QLocale::system().toString(std::get<0>(v), localeDateFormat + Settings::timeFormat);
    }
    case NameRole:
        return std::get<1>(v);
    case InfoRole:
        return std::get<2>(v);
    default:
        return {};
    }
}

QHash<int, QByteArray> FilesModel::roleNames() const
{
    return {{NameRole, "name"}, {DateTimeRole, "datetime"}, {InfoRole, "info"}};
}

QVariantMap FilesModel::get(const int index) const
{
    if (index < 0 || index >= m_files.count()) {
        return {};
    }

    const auto &v = m_files[index];
    const auto localeDateFormat = QLocale::system().dateFormat(QLocale::ShortFormat);
    const auto dt = QLocale::system().toString(std::get<0>(v), localeDateFormat + Settings::timeFormat);

    return {{"datetime", dt}, {"name", std::get<1>(v)}, {"info", std::get<2>(v)}};
}
