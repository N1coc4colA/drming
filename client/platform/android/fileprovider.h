#ifndef FILEPROVIDERPLATFORM_H
#define FILEPROVIDERPLATFORM_H

#include "../../fileprovider.h"

#include <QJniObject>

#include <functional>

template<typename T>
class JavaProxy;

namespace Platform {

/// Provides the platform file and certificate store.
class FileProvider : public ::FileProvider
{
    Q_OBJECT

    template<typename... Args>
    using Fn = std::function<Args...>;

public:
    explicit FileProvider(QObject *parent);

    /// Loads the available server certificates.
    void loadServerCerts() override;
    /// Loads the configured client credentials.
    void loadClients() override;

    /// Deletes a stored server certificate.
    void deleteServerCert(const QString &file) override;
    /// Deletes a client and its credential files.
    void deleteClient(const QString &file) override;

    /// Adds a server certificate from the selected file.
    int addServerCert() override;
    /// Adds a client certificate to client storage.
    int addClientCert(const QString &name) override;
    /// Adds a client private key to client storage.
    int addClientKey(const QString &name) override;
    /// Exports a server certificate through the Android picker.
    int exportServerCert(const QString &name) override;
    /// Exports a client certificate through the Android picker.
    int exportClientCert(const QString &name) override;
    /// Exports a client key through the Android picker.
    int exportClientKey(const QString &name) override;

    /// Returns clients with usable certificate and key files.
    QStringList validClientEntries() override;

    /// Returns the selected client certificate and key data.
    QPair<QSslCertificate, QSslKey> clientData(const QString &name) override;
    /// Returns the trusted certificate data.
    QList<QSslCertificate> trustedCerts() override;

    /// Returns the directory containing server certificates.
    QString serverCertsPath() override;
    /// Returns the directory containing client credentials.
    QString clientPath() override;
    /// Returns the standard client certificate filename.
    QString clientCertName() override;
    /// Returns the standard client key filename.
    QString clientKeyName() override;

    /// Updates the stored files for a client.
    bool updateClientEntry(const QVariantMap &map) override;
    /// Completes an Android file import operation.
    void completeFileImport(int operation, const QString &name, const QString &source);

Q_SIGNALS:
    void fileImportFinished(int operation, int result);

private:
    /// Java FileHelper instance.
    QJniObject m_object{};

    Fn<QString()> jServerCertsPath;
    Fn<QString()> jClientPath;
    Fn<QString()> jClientCertName;
    Fn<QString()> jClientKeyName;

    Fn<FilesModel::MapType()> jServerCerts;
    Fn<FilesModel::MapType()> jClients;

    Fn<bool(const QString &)> jDeleteServerCert;
    Fn<bool(const QString &)> jDeleteClient;
    Fn<int(const QString &)> jAddServerCert;
    Fn<int(const QString &, const QString &)> jAddClientCert;
    Fn<int(const QString &, const QString &)> jAddClientKey;
    Fn<bool(int, const QString &, const QString &)> jStartFilePicker;

    Fn<QByteArray(const QString &)> jClientCertData;
    Fn<QByteArray(const QString &)> jClientKeyData;

    Fn<QList<QByteArray>()> jTrustedCertsData;

    Fn<bool(const QString &, const QString &)> jUpdateClientEntry;
    Fn<QList<QString>()> jValidClientEntries;

    friend class JavaProxy<FileProvider>;
};

} // namespace Platform

#endif // FILEPROVIDERPLATFORM_H
