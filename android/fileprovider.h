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
    Q_INVOKABLE virtual void loadClientCerts() = 0;

    Q_INVOKABLE virtual void deleteServerCert(const QString &file) = 0;
    Q_INVOKABLE virtual void deleteClientCert(const QString &file) = 0;

    Q_INVOKABLE virtual void addServerCert() = 0;
    Q_INVOKABLE virtual void addClientCerts() = 0;

protected:
    FilesModel *m_clientFiles = nullptr;
    FilesModel *m_serverFiles = nullptr;

private:
    static FileProvider *m_instance;
};

#endif // FILEPROVIDER_H
