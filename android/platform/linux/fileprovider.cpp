#include "fileprovider.h"

#include <QDir>
#include <QFile>
#include <QStandardPaths>

namespace Platform {

FileProvider::FileProvider(QObject *parent)
    : ::FileProvider(parent)
{
    const auto serversLocation = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation) + '/' + folderName + "/servers";
    const auto clientsLocation = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation) + '/' + folderName + "/clients";

    QDir dir{};
    if (!dir.mkpath(serversLocation)) {
        m_dirCreationError = true;
        m_errorMessage = tr("");
        return;
    }

    if (!dir.mkpath(clientsLocation)) {
        m_dirCreationError = true;
        m_errorMessage = tr("");
    }
}

void FileProvider::loadServerCerts()
{
    const auto location = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation) + '/' + folderName + "/servers";
    const auto available = QDir(location).entryList(QDir::Files | QDir::Readable | QDir::Hidden, QDir::LocaleAware);

    qInfo() << "Looking into" << location;
    for (const auto &p : available) {
        qInfo() << p;
    }

    //m_serverFiles->setData(available);
}

void FileProvider::loadClientCerts()
{
    const auto location = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation) + '/' + folderName + "/clients";
    const auto available = QDir(location).entryList(QDir::Files | QDir::Readable | QDir::Hidden, QDir::LocaleAware);

    qInfo() << "Looking into" << location;
    for (const auto &p : available) {
        qInfo() << p;
    }

    //return available;
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
