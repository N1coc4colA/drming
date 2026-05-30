#include "fileprovider.h"

#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QStandardPaths>

#include "../../../certificatesupport.h"

namespace Platform {

FileProvider::FileProvider(QObject *parent)
    : ::FileProvider(parent)
{
    const QDir dir{};
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

QPair<QSslCertificate, QSslKey> FileProvider::clientData(const QString &name)
{
    const auto basePath = clientPath() + name + "/";
    return {openCertificate(basePath + clientCertName()), openKey(basePath + clientKeyName())};
}

QList<QSslCertificate> FileProvider::trustedCerts()
{
    return QSslCertificate::fromPath(FileProvider::instance()->serverCertsPath() + "*", QSsl::Pem, QSslCertificate::PatternSyntax::Wildcard);
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
    const auto keyName = "/" + clientKeyName();
    for (const auto &info : availables) {
        const auto name = info.fileName();
        const auto certPath = basePath + name + certName;
        const auto keyPath = basePath + name + keyName;
        const auto certExists = QFile(certPath).exists() && canOpenCert(certPath);
        const auto keyExists = QFile(keyPath).exists() && canOpenKey(keyPath);

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

int FileProvider::addServerCert()
{
    const auto sourceLocation = QFileDialog::getOpenFileName(nullptr, tr("Import certificate"), {}, "Certificate (*.crt)");
    if (sourceLocation.isEmpty()) {
        return 0;
    }

    const QFileInfo source(sourceLocation);
    const auto targetLocation = serverCertsPath() + source.fileName();

    if (!QFile::copy(sourceLocation, targetLocation)) {
        qDebug() << "Failed to copy from " << sourceLocation << "to" << targetLocation;
        // [TODO] Generate error message
        return 0;
    }

    if (!canOpenCert(targetLocation)) {
        return 1;
    }

    loadServerCerts();

    return 2;
}

bool FileProvider::createClientPathStorage(const QString &name)
{
    const auto dirPath = clientPath() + name + "/";
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
    const auto src = source.filePath();

    if (!QFile::copy(src, dst)) {
        qDebug() << "Failed to copy from " << src << "to" << dst;
        // [TODO] Generate error message
        return false;
    }

    return true;
}

int FileProvider::addClientCert(const QString &name)
{
    if (!createClientPathStorage(name)) {
        return 0;
    }

    const auto path = clientPath() + name + "/" + clientCertName();
    if (!copyFile(tr("Import certificate"), tr("Certificate (*.crt)"), path)) {
        return 0;
    }

    if (!canOpenCert(path)) {
        return 1;
    }

    loadClients();

    return 2;
}

int FileProvider::addClientKey(const QString &name)
{
    if (!createClientPathStorage(name)) {
        return 0;
    }

    const auto path = clientPath() + name + "/" + clientKeyName();
    if (!copyFile(tr("Import key"), tr("Key (*.key)"), path)) {
        return 0;
    }

    if (!canOpenKey(path)) {
        return 1;
    }

    loadClients();

    return 2;
}

QStringList FileProvider::validClientEntries()
{
    QStringList out{};
    for (const auto &entry : m_clientFiles->internalData()) {
        const auto &map = std::get<2>(entry);
        if (map["cert"].toBool() && map["key"].toBool()) {
            out.append(std::get<1>(entry));
        }
    }

    return out;
}

bool FileProvider::updateClientEntry(const QVariantMap &map)
{
    if (!map.contains("previousName") || !map.contains("updatedName")) {
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

    loadClients();

    return true;
}

} // namespace Platform
