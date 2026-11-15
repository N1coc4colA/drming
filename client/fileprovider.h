#ifndef FILEPROVIDER_H
#define FILEPROVIDER_H

#include <QCoreApplication>
#include <QSslCertificate>
#include <QSslKey>

#include "models/filesmodel.h"

/**
 * Platform-neutral certificate and key storage interface.
 *
 * Concrete providers expose the same model and operations to QML while
 * choosing filesystem locations and native file pickers per platform.
 */
class FileProvider : public QObject
{
    Q_OBJECT

public:
    explicit FileProvider(QObject *parent);
    virtual ~FileProvider();

    /// Returns the platform provider singleton, creating it on demand.
    static FileProvider *instance(QObject *parent = qApp);

    /// Loads certificates trusted for authenticating servers.
    Q_INVOKABLE virtual void loadServerCerts() = 0;
    /// Loads client certificate and key entries.
    Q_INVOKABLE virtual void loadClients() = 0;

    /// Removes a trusted server certificate.
    Q_INVOKABLE virtual void deleteServerCert(const QString &fileName) = 0;
    /// Removes a named client certificate and key entry.
    Q_INVOKABLE virtual void deleteClient(const QString &name) = 0;

    /// Adds a trusted server certificate using the platform picker.
    Q_INVOKABLE virtual int addServerCert() = 0;
    /// Adds a client certificate for the named entry.
    Q_INVOKABLE virtual int addClientCert(const QString &name) = 0;
    /// Adds a client key for the named entry.
    Q_INVOKABLE virtual int addClientKey(const QString &name) = 0;
    /// Exports a trusted server certificate.
    Q_INVOKABLE virtual int exportServerCert(const QString &name) = 0;
    /// Exports a client certificate.
    Q_INVOKABLE virtual int exportClientCert(const QString &name) = 0;
    /// Exports a client private key.
    Q_INVOKABLE virtual int exportClientKey(const QString &name) = 0;

    /// Returns names that have both usable client files.
    Q_INVOKABLE virtual QStringList validClientEntries() = 0;

    /// Returns the certificate and key for a named client entry.
    virtual QPair<QSslCertificate, QSslKey> clientData(const QString &name) = 0;
    /// Returns certificates trusted for server authentication.
    virtual QList<QSslCertificate> trustedCerts() = 0;

    /// Returns the directory containing trusted server certificates.
    virtual QString serverCertsPath() = 0;
    /// Returns the directory containing client credentials.
    virtual QString clientPath() = 0;
    /// Returns the conventional client certificate filename.
    virtual QString clientCertName() = 0;
    /// Returns the conventional client key filename.
    virtual QString clientKeyName() = 0;

    /// Updates or creates a client entry from its QML map.
    Q_INVOKABLE virtual bool updateClientEntry(const QVariantMap &map) = 0;

    /// Indicates whether provider directories could not be created.
    [[nodiscard]] bool hasError() const
    {
        return m_dirCreationError;
    }
    /// Returns the most recent provider error message.
    [[nodiscard]] QString errorMessage() const
    {
        return m_errorMessage;
    }

    /// Lists client certificate/key pairs currently available to the user.
    Q_INVOKABLE FilesModel *clientCertsModel()
    {
        return m_clientFiles;
    }
    Q_INVOKABLE const FilesModel *clientCertsModel() const
    {
        return m_clientFiles;
    }
    /// Lists certificates that can be used to authenticate the server.
    Q_INVOKABLE FilesModel *serverCertsModel()
    {
        return m_serverFiles;
    }
    Q_INVOKABLE const FilesModel *serverCertsModel() const
    {
        return m_serverFiles;
    }

protected:
    /// Human-readable description of the most recent provider error.
    QString m_errorMessage{};
    /// Client certificate and key metadata exposed to QML.
    FilesModel *m_clientFiles = nullptr;
    /// Trusted server certificate metadata exposed to QML.
    FilesModel *m_serverFiles = nullptr;
    /// Whether creating provider directories failed.
    bool m_dirCreationError = false;

private:
    /// Process-wide provider instance.
    static FileProvider *m_instance;
};

#endif // FILEPROVIDER_H
