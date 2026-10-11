#ifndef CERTIFICATE_SUPPORT_H
#define CERTIFICATE_SUPPORT_H

#include <QFile>
#include <QSslCertificate>
#include <QSslConfiguration>
#include <QSslKey>

inline QSslCertificate openCertificateFromData(const QByteArray &data)
{
    const auto certs = QSslCertificate::fromData(data, QSsl::Pem);
    if (certs.isEmpty()) {
        qCritical() << "Failed to read certificate from data.";
        return QSslCertificate("");
    }

    return certs.first();
}

inline QSslCertificate openCertificate(const QString &path)
{
    const auto certs = QSslCertificate::fromFile(path, QSsl::Pem);
    if (certs.isEmpty()) {
        qCritical() << "Failed to open certificate:" << path;
        return QSslCertificate("");
    }

    return certs.first();
}

inline auto openCertificateFiles(const QString &path)
{
    return QSslCertificate::fromPath(path, QSsl::Pem, QSslCertificate::PatternSyntax::Wildcard);
}

inline QSslKey keyFromData(const QByteArray &keyData)
{
    const QSslKey serverKey(keyData, QSsl::Rsa, QSsl::Pem, QSsl::PrivateKey);
    return !serverKey.isNull() ? serverKey : QSslKey(keyData, QSsl::Ec, QSsl::Pem, QSsl::PrivateKey);
}

inline QSslKey openKey(const QString &path)
{
    QFile keyFile(path);
    if (!keyFile.open(QIODevice::ReadOnly)) {
        qCritical() << "Failed to open private key:" << path;
        return {};
    }

    return keyFromData(keyFile.readAll());
}

inline bool canOpenCert(const QString &path)
{
    return !openCertificate(path).isNull();
}

inline bool canOpenKey(const QString &path)
{
    return !openKey(path).isNull();
}

bool loadServerCertsConfig(QSslConfiguration &outConfig, const QString &protocol, const QString &srvCertPath, const QString &srvKeyPath)
{
    const auto serverCert = openCertificate(srvCertPath);
    const auto serverKey = openKey(srvKeyPath);
    if (serverKey.isNull() || serverCert.isNull()) [[unlikely]] {
        return false;
    }

    auto conf = protocol == "dtls" ? QSslConfiguration::defaultDtlsConfiguration() : QSslConfiguration::defaultConfiguration();
    conf.setLocalCertificate(serverCert);
    conf.setPrivateKey(serverKey);
    if (protocol == "dtls") {
        conf.setDtlsCookieVerificationEnabled(false);
    }

    outConfig = conf;

    return true;
}

#endif // CERTIFICATE_SUPPORT_H
