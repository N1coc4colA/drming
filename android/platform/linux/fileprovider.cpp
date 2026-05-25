#include "fileprovider.h"

#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QStandardPaths>

namespace Platform {

FileProvider::FileProvider(QObject *parent)
    : ::FileProvider(parent)
{
    QDir dir{};
    if (!dir.mkpath(FileProvider::serverCertsPath())) {
        m_dirCreationError = true;
        m_errorMessage = tr("Failed to create servers certificates storage.");
        qDebug() << m_errorMessage;
        return;
    }

    if (!dir.mkpath(FileProvider::clientCertsPath())) {
        m_dirCreationError = true;
        m_errorMessage = tr("Failed to create client keys storage.");
        qDebug() << m_errorMessage;
    }

    // [TODO] Handle error visibility in QML.
}

QString FileProvider::serverCertsPath()
{
    return QStandardPaths::writableLocation(QStandardPaths::ConfigLocation) + '/' + folderName + "/servers/";
}

QString FileProvider::clientCertsPath()
{
    return QStandardPaths::writableLocation(QStandardPaths::ConfigLocation) + '/' + folderName + "/clients/";
}

void FileProvider::loadServerCerts()
{
    const auto availables = QDir(serverCertsPath()).entryInfoList(QDir::Files | QDir::Readable | QDir::Hidden, QDir::LocaleAware);

    QList<QPair<QDateTime, QString>> data{};
    data.reserve(availables.size());

    for (const auto &info : availables) {
        data.append({info.fileTime(QFileDevice::FileModificationTime), info.fileName()});
    }

    m_serverFiles->setData(data);
}

void FileProvider::loadClientCerts()
{
    const auto availables = QDir(clientCertsPath()).entryInfoList(QDir::Files | QDir::Readable | QDir::Hidden, QDir::LocaleAware);

    QList<QPair<QDateTime, QString>> data{};
    data.reserve(availables.size());

    for (const auto &info : availables) {
        data.append({info.fileTime(QFileDevice::FileModificationTime), info.fileName()});
    }

    m_clientFiles->setData(data);
}

void FileProvider::deleteServerCert(const QString &fileName)
{
    QFile file(serverCertsPath() + fileName);

    if (!file.remove()) {
        qDebug() << "Failed to delete " << file.fileName();
        // [TODO] Generate error message.
        return;
    }

    loadServerCerts();
}

void FileProvider::deleteClientCert(const QString &fileName)
{
    QFile file(clientCertsPath() + fileName);

    if (!file.remove()) {
        qDebug() << "Failed to delete " << file.fileName();
        // [TODO] Generate error message.
        return;
    }

    loadClientCerts();
}

void FileProvider::addServerCert()
{
    const auto sourceLocation = QFileDialog::getOpenFileName(nullptr, tr("Import certificate"), {}, "Certificate (*.crt)");
    if (sourceLocation.isEmpty()) {
        return;
    }

    const QFileInfo source(sourceLocation);
    const auto targetLocation = serverCertsPath() + source.fileName();

    if (!QFile::copy(sourceLocation, targetLocation)) {
        qDebug() << "Failed to copy from " << sourceLocation << "to" << targetLocation;
        // [TODO] Generate error message
        return;
    }

    loadServerCerts();
}

void FileProvider::addClientCert()
{
    const auto sourceLocation = QFileDialog::getOpenFileName(nullptr, tr("Import key"), {}, "Key (*.key)");
    if (sourceLocation.isEmpty()) {
        return;
    }

    const QFileInfo source(sourceLocation);
    const auto targetLocation = clientCertsPath() + source.fileName();

    if (!QFile::copy(sourceLocation, targetLocation)) {
        qDebug() << "Failed to copy from " << sourceLocation << "to" << targetLocation;
        // [TODO] Generate error message
        return;
    }

    loadClientCerts();
}

} // namespace Platform
