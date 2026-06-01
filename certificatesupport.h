#ifndef CERTIFICATE_SUPPORT_H
#define CERTIFICATE_SUPPORT_H

#include <QFile>
#include <QSslCertificate>
#include <QSslKey>

inline QSslCertificate openCertificate(const QString &path)
{
    const auto certs = QSslCertificate::fromFile(path, QSsl::Pem);
    if (certs.isEmpty()) {
        qCritical() << "Failed to open certificate:" << path;
        return QSslCertificate("");
    }

    return certs.first();
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

#endif // CERTIFICATE_SUPPORT_H
