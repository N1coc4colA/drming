#ifndef FILEPROVIDERPLATFORM_H
#define FILEPROVIDERPLATFORM_H

#include "../../fileprovider.h"

namespace Platform {

/// Provides the platform file and certificate store.
class FileProvider : public ::FileProvider
{
    Q_OBJECT

public:
    explicit FileProvider(QObject *parent);

    /// Loads the available server certificates.
    void loadServerCerts() override;
    /// Loads the configured client credentials.
    void loadClients() override;

    /// Deletes a stored server certificate.
    void deleteServerCert(const QString &fileName) override;
    /// Deletes a client and its credential files.
    void deleteClient(const QString &name) override;

    /// Adds a server certificate from the selected file.
    int addServerCert() override;
    /// Adds a client certificate to client storage.
    int addClientCert(const QString &name) override;
    /// Adds a client private key to client storage.
    int addClientKey(const QString &name) override;
    /// Exports a server certificate to a user-selected path.
    int exportServerCert(const QString &name) override;
    /// Exports a client certificate to a user-selected path.
    int exportClientCert(const QString &name) override;
    /// Exports a client key to a user-selected path.
    int exportClientKey(const QString &name) override;

    /// Returns clients with usable certificate and key files.
    QStringList validClientEntries() override;

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

    /// Returns the selected client certificate and key data.
    QPair<QSslCertificate, QSslKey> clientData(const QString &name) override;
    /// Returns the trusted certificate data.
    QList<QSslCertificate> trustedCerts() override;

private:
    /// Creates the storage directory for a client.
    bool createClientPathStorage(const QString &name);
    /// Prompts the user with a file dialog and copies the selected file to dst.
    bool copyFile(const QString &title, const QString &filter, const QString &dst);

    /// Storage subdirectory name under user application data.
    static constexpr auto folderName = "drming";
};

} // namespace Platform

#endif // FILEPROVIDERPLATFORM_H
