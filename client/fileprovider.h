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

    static FileProvider *instance(QObject *parent = qApp);

    Q_INVOKABLE virtual void loadServerCerts() = 0;
    Q_INVOKABLE virtual void loadClients() = 0;

    Q_INVOKABLE virtual void deleteServerCert(const QString &fileName) = 0;
    Q_INVOKABLE virtual void deleteClient(const QString &name) = 0;

    Q_INVOKABLE virtual int addServerCert() = 0;
    Q_INVOKABLE virtual int addClientCert(const QString &name) = 0;
    Q_INVOKABLE virtual int addClientKey(const QString &name) = 0;
    Q_INVOKABLE virtual int exportServerCert(const QString &name) = 0;
    Q_INVOKABLE virtual int exportClientCert(const QString &name) = 0;
    Q_INVOKABLE virtual int exportClientKey(const QString &name) = 0;

    Q_INVOKABLE virtual QStringList validClientEntries() = 0;

    virtual QPair<QSslCertificate, QSslKey> clientData(const QString &name) = 0;
    virtual QList<QSslCertificate> trustedCerts() = 0;

    virtual QString serverCertsPath() = 0;
    virtual QString clientPath() = 0;
    virtual QString clientCertName() = 0;
    virtual QString clientKeyName() = 0;

    Q_INVOKABLE virtual bool updateClientEntry(const QVariantMap &map) = 0;

    [[nodiscard]] bool hasError() const
    {
        return m_dirCreationError;
    }
    [[nodiscard]] QString errorMessage() const
    {
        return m_errorMessage;
    }

    /// Lists client certificate/key pairs currently available to the user.
    Q_INVOKABLE FilesModel *clientCertsModel()
    {
        return m_clientFiles;
    }
    /// Lists certificates that can be used to authenticate the server.
    Q_INVOKABLE FilesModel *serverCertsModel()
    {
        return m_serverFiles;
    }

protected:
    QString m_errorMessage{};
    FilesModel *m_clientFiles = nullptr;
    FilesModel *m_serverFiles = nullptr;
    bool m_dirCreationError = false;

private:
    static FileProvider *m_instance;
};

#endif // FILEPROVIDER_H
