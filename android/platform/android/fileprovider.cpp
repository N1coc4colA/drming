#include "fileprovider.h"

#include <QFileDialog>

#include "../../../certificatesupport.h"

#include "native.h"

namespace Platform {

FileProvider::FileProvider(QObject *parent)
    : ::FileProvider(parent)
{
    createNativeObject_FileHelper(*this);
}

void FileProvider::loadServerCerts()
{
    m_serverFiles->setData(jServerCerts());
}

void FileProvider::loadClients()
{
    m_clientFiles->setData(jClients());
}

void FileProvider::deleteServerCert(const QString &file)
{
    jDeleteServerCert(file);
    loadServerCerts();
}

void FileProvider::deleteClient(const QString &file)
{
    jDeleteClient(file);
    loadClients();
}

int FileProvider::addServerCert()
{
    const auto sourceLocation = QFileDialog::getOpenFileName(nullptr, tr("Import certificate"), {}, "application/x-x509-ca-cert");
    if (sourceLocation.isEmpty()) {
        return false;
    }

    const QFileInfo source(sourceLocation);
    const auto src = source.filePath();

    const auto ret = jAddServerCert(src);
    if (ret < 2) {
        return ret;
    }

    loadServerCerts();

    return 2;
}

int FileProvider::addClientCert(const QString &name)
{
    const auto sourceLocation = QFileDialog::getOpenFileName(nullptr, tr("Import certificate"), {}, tr("application/x-x509-ca-cert"));
    if (sourceLocation.isEmpty()) {
        return false;
    }

    const QFileInfo source(sourceLocation);
    const auto src = source.filePath();

    const auto ret = jAddClientCert(name, src);
    if (ret < 2) {
        return ret;
    }

    loadClients();

    return 2;
}

int FileProvider::addClientKey(const QString &name)
{
    const auto sourceLocation = QFileDialog::getOpenFileName(nullptr, tr("Import key"), {}, tr("application/x-pem-key"));
    if (sourceLocation.isEmpty()) {
        return false;
    }

    const QFileInfo source(sourceLocation);
    const auto src = source.filePath();

    const auto ret = jAddClientKey(name, src);
    if (ret < 2) {
        return ret;
    }

    loadClients();

    return 2;
}

QStringList FileProvider::validClientEntries()
{
    return jValidClientEntries();
}

QPair<QSslCertificate, QSslKey> FileProvider::clientData(const QString &name)
{
    const auto certData = jClientCertData(name);
    const auto keyData = jClientKeyData(name);

    return {QSslCertificate::fromData(certData, QSsl::Pem).first(), keyFromData(keyData)};
}

QList<QSslCertificate> FileProvider::trustedCerts()
{
    const auto certsData = jTrustedCertsData();

    QList<QSslCertificate> out{};
    out.reserve(certsData.size());

    for (const auto &certData : certsData) {
        out.append(QSslCertificate::fromData(certData, QSsl::Pem).first());
    }

    return out;
}

QString FileProvider::serverCertsPath()
{
    return jServerCertsPath();
}

QString FileProvider::clientPath()
{
    return jClientPath();
}

QString FileProvider::clientCertName()
{
    return jClientCertName();
}

QString FileProvider::clientKeyName()
{
    return jClientKeyName();
}

bool FileProvider::updateClientEntry(const QVariantMap &map)
{
    if (!map.contains("previousName") || !map.contains("updatedName")) {
        return false;
    }

    const auto src = clientPath() + map["previousName"].toString();
    const auto dst = clientPath() + map["updatedName"].toString();

    if (dst.isEmpty() || !jUpdateClientEntry(src, dst)) {
        return false;
    }

    loadClients();

    return true;
}

} // namespace Platform
