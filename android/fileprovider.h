#ifndef FILEPROVIDER_H
#define FILEPROVIDER_H

#include <QObject>

#include "models/filesmodel.h"

class FileProvider : public QObject
{
    Q_OBJECT

public:
    explicit FileProvider(QObject *parent);

    static FileProvider *instance();

    Q_INVOKABLE virtual void loadServerCerts() = 0;
    Q_INVOKABLE virtual void loadClients() = 0;

    Q_INVOKABLE virtual void deleteServerCert(const QString &fileName) = 0;
    Q_INVOKABLE virtual void deleteClient(const QString &name) = 0;

    Q_INVOKABLE virtual void addServerCert() = 0;
    Q_INVOKABLE virtual void addClientCert(const QString &name) = 0;
    Q_INVOKABLE virtual void addClientKey(const QString &name) = 0;

    virtual QString serverCertsPath() = 0;
    virtual QString clientPath() = 0;
    virtual QString clientCertName() = 0;
    virtual QString clientKeyName() = 0;

    Q_INVOKABLE virtual bool updateClientEntry(const QVariantMap &map) = 0;

    inline bool hasError() const { return m_dirCreationError; }
    inline QString errorMessage() const { return m_errorMessage; }

    Q_INVOKABLE FilesModel *clientCertsModel() { return m_clientFiles; }
    Q_INVOKABLE FilesModel *serverCertsModel() { return m_serverFiles; }

protected:
    QString m_errorMessage{};
    FilesModel *m_clientFiles = nullptr;
    FilesModel *m_serverFiles = nullptr;
    bool m_dirCreationError = false;

private:
    static FileProvider *m_instance;
};

#endif // FILEPROVIDER_H
