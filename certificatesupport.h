#ifndef CERTIFICATE_SUPPORT_H
#define CERTIFICATE_SUPPORT_H

#include <QSslCertificate>
#include <QSslKey>

bool canOpenCert(const QString &path)
{
    const auto cert = QSslCertificate::fromFile(path);
    if (!std::all_of(cert.cbegin(), cert.cend(), [](const auto &cert) { return !cert.isNull(); })) {
        return false;
    }

    return true;
}

bool canOpenKey(const QString &path)
{
    QFile keyFile(path);
    if (!keyFile.open(QIODevice::ReadOnly)) {
        qCritical() << "Failed to open server private key:" << path;
        return false;
    }
    const QByteArray keyData = keyFile.readAll();

    QSslKey serverKey(keyData, QSsl::Rsa, QSsl::Pem, QSsl::PrivateKey);
    if (serverKey.isNull()) {
        serverKey = QSslKey(keyData, QSsl::Ec, QSsl::Pem, QSsl::PrivateKey);
    }

    if (serverKey.isNull()) {
        qCritical() << "Failed to parse server private key:" << path;
        return false;
    }

    return true;
}

#endif // CERTIFICATE_SUPPORT_H
