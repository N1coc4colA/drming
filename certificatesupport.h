#ifndef CERTIFICATE_SUPPORT_H
#define CERTIFICATE_SUPPORT_H

#include <QFile>
#include <QSslCertificate>
#include <QSslKey>

inline QSslCertificate openCertificate(const QString &path)
{
    const auto certs = QSslCertificate::fromFile(path);
    if (certs.isEmpty()) {
        qCritical() << "Failed to open certificate:" << path;
        return QSslCertificate("");
    }

    return certs.first();
}

inline QSslKey openKey(const QString &path)
{
    QFile keyFile(path);
    if (!keyFile.open(QIODevice::ReadOnly)) {
        qCritical() << "Failed to open private key:" << path;
        return {};
    }
    const auto keyData = keyFile.readAll();
    const QSslKey serverKey(keyData, QSsl::Rsa, QSsl::Pem, QSsl::PrivateKey);

    return !serverKey.isNull() ? serverKey : QSslKey(keyData, QSsl::Ec, QSsl::Pem, QSsl::PrivateKey);
}

inline bool canOpenCert(const QString &path)
{
    return !openCertificate(path).isNull();
}

inline bool canOpenKey(const QString &path)
{
    return !openKey(path).isNull();
}

#endif // CERTIFICATE_SUPPORT_H
