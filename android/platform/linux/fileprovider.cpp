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

    if (!dir.mkpath(FileProvider::clientPath())) {
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

QString FileProvider::clientPath()
{
    return QStandardPaths::writableLocation(QStandardPaths::ConfigLocation) + '/' + folderName + "/clients/";
}

QString FileProvider::clientCertName()
{
    return "cert.crt";
}

QString FileProvider::clientKeyName()
{
    return "key.key";
}

void FileProvider::loadServerCerts()
{
    const auto availables = QDir(serverCertsPath()).entryInfoList(QDir::Files | QDir::Readable | QDir::Hidden, QDir::LocaleAware);

    QList<std::tuple<QDateTime, QString, QVariantMap>> data{};
    data.reserve(availables.size());

    for (const auto &info : availables) {
        data.append({info.fileTime(QFileDevice::FileModificationTime), info.fileName(), {}});
    }

    m_serverFiles->setData(data);
}

void FileProvider::loadClients()
{
    const auto availables = QDir(clientPath()).entryInfoList(QDir::Dirs | QDir::Readable | QDir::Hidden | QDir::NoDotAndDotDot, QDir::LocaleAware);

    QList<std::tuple<QDateTime, QString, QVariantMap>> data{};
    data.reserve(availables.size());

    const auto basePath = clientPath();
    const auto certName = "/" + clientCertName();
    const auto keyName = "/" + clientCertName();
    for (const auto &info : availables) {
        const auto name = info.fileName();
        const auto certExists = QFile(basePath + name + certName).exists();
        const auto keyExists = QFile(basePath + name + keyName).exists();

        data.append({info.fileTime(QFileDevice::FileModificationTime), name, {{"cert", certExists}, {"key", keyExists}}});
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

void FileProvider::deleteClient(const QString &name)
{
    QDir dir(clientPath() + name);

    if (!dir.removeRecursively()) {
        qDebug() << "Failed to delete " << dir.absolutePath();
        // [TODO] Generate error message.
        return;
    }

    loadClients();
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

bool FileProvider::createClientPathStorage(const QString &name)
{
    const QString dirPath = clientPath() + name + "/";
    const QDir dir(dirPath);
    if (!dir.exists() && !dir.mkpath(dirPath)) {
        qDebug() << "Failed to create directory for client key & cert!";
        return false;
    }

    return true;
}

bool FileProvider::copyFile(const QString &title, const QString &filter, const QString &dst)
{
    const auto sourceLocation = QFileDialog::getOpenFileName(nullptr, title, {}, filter);
    if (sourceLocation.isEmpty()) {
        return false;
    }

    const QFileInfo source(sourceLocation);
    const QString src = source.filePath();

    if (!QFile::copy(src, dst)) {
        qDebug() << "Failed to copy from " << src << "to" << dst;
        // [TODO] Generate error message
        return false;
    }

    return true;
}

void FileProvider::addClientCert(const QString &name)
{
    if (!createClientPathStorage(name)) {
        return;
    }

    if (!copyFile(tr("Import certificate"), tr("Certificate (*.crt)"), clientPath() + name + "/" + clientCertName())) {
        return;
    }

    loadClients();
}

void FileProvider::addClientKey(const QString &name)
{
    if (!createClientPathStorage(name)) {
        return;
    }

    if (!copyFile(tr("Import key"), tr("Key (*.key)"), clientPath() + name + "/" + clientKeyName())) {
        return;
    }

    loadClients();
}

bool FileProvider::updateClientEntry(const QVariantMap &map)
{
    if (!map.contains("previousName") || map.contains("updatedName")) {
        return false;
    }

    const auto src = clientPath() + map["previousName"].toString();
    const auto dst = clientPath() + map["updatedName"].toString();

    if (dst.isEmpty()) {
        return false;
    }

    if (src.isEmpty()) {
        if (!createClientPathStorage(map["updatedName"].toString())) {
            return false;
        }
    } else if (!QDir().rename(src, dst)) {
        qDebug() << "Failed to move dir from" << src << "to" << dst;
        // [TODO] Generate error
        return false;
    }

    return true;
}

} // namespace Platform
