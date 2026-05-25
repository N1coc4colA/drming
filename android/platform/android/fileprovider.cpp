#include "fileprovider.h"

namespace Platform {

FileProvider::FileProvider(QObject *parent)
    : ::FileProvider(parent)
{}

QList<QString> FileProvider::loadServerCerts()
{
    return {};
}

QList<QString> FileProvider::loadClientCerts()
{
    return {};
}

void FileProvider::deleteServerCert(const QString &file)
{
    Q_UNUSED(file);
}

void FileProvider::deleteClientCert(const QString &file)
{
    Q_UNUSED(file);
}

void FileProvider::addServerCert() {}

void FileProvider::addClientCerts() {}

} // namespace Platform
