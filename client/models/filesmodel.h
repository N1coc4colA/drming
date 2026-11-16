#ifndef FILESMODEL_H
#define FILESMODEL_H

#include <QAbstractListModel>
#include <QDateTime>
#include <QVariantMap>

/**
 * Presents certificate metadata to QML using stable role names.
 *
 * The model owns a snapshot supplied by a FileProvider; callers should replace
 * the snapshot through setData rather than mutate internal entries.
 */
class FilesModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged)

public:
    /// Tuple containing modification time, display name, and metadata.
    using MapType = QList<std::tuple<QDateTime, QString, QVariantMap>>;

    /// Roles exposed by each certificate-file row.
    enum FileRoles {
        /// Display name of the file or entry.
        NameRole = Qt::UserRole + 1,
        /// Localized file modification date and time.
        DateTimeRole,
        /// Additional provider metadata.
        InfoRole
    };

    /// Owns the file snapshot displayed by QML.
    explicit FilesModel(QObject *parent = nullptr);

    /// Supplies the number of rows available to a view.
    [[nodiscard]] int rowCount(const QModelIndex &parent) const override;
    /// Maps a file row and role to its QML value.
    [[nodiscard]] QVariant data(const QModelIndex &index, int role) const override;
    /// Names the metadata fields consumed by QML.
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

    /// Reports how many files are in the current snapshot.
    Q_INVOKABLE [[nodiscard]] int count() const
    {
        return static_cast<int>(m_files.count());
    }
    /// Returns one file row as a QML-friendly map.
    Q_INVOKABLE [[nodiscard]] QVariantMap get(int index) const;

    /// Replaces the visible file snapshot and emits the required model changes.
    void setContent(const MapType &newData);

    /// Exposes the current snapshot to provider implementations.
    [[nodiscard]] const MapType &internalData() const
    {
        return m_files;
    }

Q_SIGNALS:
    /// Emitted after the number of visible files changes.
    void countChanged();

private:
    /// File metadata currently visible to the user.
    MapType m_files{};
};

#endif // FILESMODEL_H
