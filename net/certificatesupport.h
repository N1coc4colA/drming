#ifndef CERTIFICATE_SUPPORT_H
#define CERTIFICATE_SUPPORT_H

#include <QFile>
#include <QSslCertificate>
#include <QSslConfiguration>
#include <QSslKey>

/// Parses a PEM certificate from raw byte data.
inline QSslCertificate openCertificateFromData(const QByteArray &data)
{
    const auto certs = QSslCertificate::fromData(data, QSsl::Pem);
    if (certs.isEmpty()) {
        qCritical() << "Failed to read certificate from data.";
        return QSslCertificate("");
    }

    return certs.first();
}

/// Loads a PEM certificate from the specified file path.
inline QSslCertificate openCertificate(const QString &path)
{
    const auto certs = QSslCertificate::fromFile(path, QSsl::Pem);
    if (certs.isEmpty()) {
        qCritical() << "Failed to open certificate:" << path;
        return QSslCertificate("");
    }

    return certs.first();
}

/// Loads all certificates matching a wildcard file path pattern.
inline auto openCertificateFiles(const QString &path)
{
    return QSslCertificate::fromPath(path, QSsl::Pem, QSslCertificate::PatternSyntax::Wildcard);
}

/// Parses a private key (RSA or EC) from raw PEM byte data.
inline QSslKey keyFromData(const QByteArray &keyData)
{
    const QSslKey serverKey(keyData, QSsl::Rsa, QSsl::Pem, QSsl::PrivateKey);
    return !serverKey.isNull() ? serverKey : QSslKey(keyData, QSsl::Ec, QSsl::Pem, QSsl::PrivateKey);
}

/// Loads a private key from the specified file path.
inline QSslKey openKey(const QString &path)
{
    QFile keyFile(path);
    if (!keyFile.open(QIODevice::ReadOnly)) {
        qCritical() << "Failed to open private key:" << path;
        return {};
    }

    return keyFromData(keyFile.readAll());
}

/// Checks whether a valid certificate can be read from the given path.
inline bool canOpenCert(const QString &path)
{
    return !openCertificate(path).isNull();
}

/// Checks whether a valid private key can be read from the given path.
inline bool canOpenKey(const QString &path)
{
    return !openKey(path).isNull();
}

/// Builds a DTLS SSL configuration from server certificate and private key paths.
inline std::optional<QSslConfiguration> loadServerCertsConfig(const QString &srvCertPath, const QString &srvKeyPath)
{
    const auto serverCert = openCertificate(srvCertPath);
    const auto serverKey = openKey(srvKeyPath);
    if (serverKey.isNull() || serverCert.isNull()) [[unlikely]] {
        return {};
    }

    auto conf = QSslConfiguration::defaultDtlsConfiguration();
    conf.setLocalCertificate(serverCert);
    conf.setPrivateKey(serverKey);
    conf.setDtlsCookieVerificationEnabled(false);
    return conf;
}

#endif // CERTIFICATE_SUPPORT_H
