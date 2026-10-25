#include "fileprovider.h"

#include "../net/certificatesupport.h"

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
    // [TODO] Generate error on j failure
    jDeleteServerCert(file);
    loadServerCerts();
}

void FileProvider::deleteClient(const QString &file)
{
    // [TODO] Generate error on j failure
    jDeleteClient(file);
    loadClients();
}

int FileProvider::addServerCert()
{
    return jStartFilePicker(0, {}, "application/x-x509-ca-cert") ? -1 : 0;
}

int FileProvider::addClientCert(const QString &name)
{
    return jStartFilePicker(1, name, "application/x-x509-ca-cert") ? -1 : 0;
}

int FileProvider::addClientKey(const QString &name)
{
    return jStartFilePicker(2, name, "application/x-pem-key") ? -1 : 0;
}

void FileProvider::completeFileImport(int operation, const QString &name, const QString &source)
{
    if (source.isEmpty()) {
        Q_EMIT fileImportFinished(operation, 0);
        return;
    }

    const auto result = operation == 0 ? jAddServerCert(source)
        : operation == 1 ? jAddClientCert(name, source)
                        : jAddClientKey(name, source);
    if (result >= 2) {
        operation == 0 ? loadServerCerts() : loadClients();
    }
    Q_EMIT fileImportFinished(operation, result);
}

QStringList FileProvider::validClientEntries()
{
    return jValidClientEntries();
}

QPair<QSslCertificate, QSslKey> FileProvider::clientData(const QString &name)
{
    return { openCertificateFromData(jClientCertData(name)), keyFromData(jClientKeyData(name)) };
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

    const auto src = map["previousName"].toString();
    const auto dst = map["updatedName"].toString();

    if (dst.isEmpty() || !jUpdateClientEntry(src, dst)) {
        return false;
    }

    loadClients();

    return true;
}

} // namespace Platform
