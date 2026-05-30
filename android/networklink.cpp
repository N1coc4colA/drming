#include "networklink.h"

#include <QBuffer>
#include <QDebug>
#include <QDir>
#include <QSslCertificate>
#include <QSslConfiguration>
#include <QtEndian>

#include "fileprovider.h"

NetworkLink::NetworkLink(QObject *parent)
    : QObject{parent}
    , Packets::Parser<NetworkLink>(*this)
    , m_socket(new QSslSocket(this))
{
    // Disable all default CA verification — we do our own allowlist check
    QSslConfiguration conf = QSslConfiguration::defaultConfiguration();
    conf.setCaCertificates({});
    conf.setPeerVerifyMode(QSslSocket::VerifyNone); // Avoid chain validation
    m_socket->setSslConfiguration(conf);

    QAbstractSocket::connect(m_socket, &QSslSocket::connected, this, &NetworkLink::onConnected);
    QAbstractSocket::connect(m_socket, &QSslSocket::disconnected, this, &NetworkLink::onDisconnected);
    QAbstractSocket::connect(m_socket, &QSslSocket::readyRead, this, &NetworkLink::onDataAvailable);
    QAbstractSocket::connect(m_socket, &QSslSocket::errorOccurred, this, &NetworkLink::onError);
    QAbstractSocket::connect(m_socket, &QSslSocket::encrypted, this, &NetworkLink::onConnected);
    QAbstractSocket::connect(m_socket, &QSslSocket::sslErrors, this, &NetworkLink::onSslErrors);

    m_socket->setSocketOption(QAbstractSocket::LowDelayOption, 1);
    m_socket->setSocketOption(QAbstractSocket::KeepAliveOption, 1);
}

NetworkLink::~NetworkLink()
{
    m_socket->close();
}

void NetworkLink::close()
{
    if (m_socket->state() == QAbstractSocket::UnconnectedState) {
        return;
    }

    m_socket->close();
}

void NetworkLink::connect(const QString &address, const int port, const QString &clientName)
{
    if (m_socket->state() != QAbstractSocket::UnconnectedState) {
        return;
    }

    qInfo() << "Connecting to:" << address << port;

    // Use encrypted connection
    const auto clientData = FileProvider::instance()->clientData(clientName);

    m_socket->setLocalCertificate(clientData.first);
    m_socket->setPrivateKey(clientData.second);
    m_socket->connectToHostEncrypted(address, port);
}

void NetworkLink::onError(const QAbstractSocket::SocketError error)
{
    qWarning() << "Connection error occurred: " << error;
    if (m_socket) {
        Q_EMIT NetworkLink::error(m_socket->errorString());
    } else {
        Q_EMIT NetworkLink::error(tr("A network error occurred."));
    }
}

void NetworkLink::onConnected()
{
    qInfo() << "Connected (encrypted:" << m_socket->isEncrypted() << ")";
    if (m_socket->isEncrypted()) {
        Q_EMIT opened();
    }
}

void NetworkLink::onDisconnected()
{
    m_socket->close();
    m_buffer.clear();
}

void NetworkLink::onDataAvailable()
{
    addData(m_socket->readAll());
}

void NetworkLink::onSslErrors(const QList<QSslError> &errors)
{
    const QSslCertificate serverCert = m_socket->peerCertificate();

    if (serverCert.isNull()) {
        qWarning() << "Server provided no certificate, aborting.";
        m_socket->abort();
        return;
    }

    QList<QSslCertificate> trustedCerts(
        QSslCertificate::fromPath(FileProvider::instance()->serverCertsPath() + "*", QSsl::Pem, QSslCertificate::PatternSyntax::Wildcard));

    if (trustedCerts.isEmpty()) {
        qWarning() << "No trusted certificates found in ./certs/valids/";
    } else {
        qInfo() << "Loaded" << trustedCerts.size() << "trusted certificate(s) from ./certs/valids/";
    }

    if (!trustedCerts.contains(serverCert)) {
        qWarning() << "Server certificate is not in the trusted list, aborting.";
        m_socket->abort();
        return;
    }

    // Cert is in our allowlist — we only tolerate hostname mismatch errors.
    // Chain/expiry/revocation errors are still fatal.
    QList<QSslError> ignorable;
    for (const QSslError &e : errors) {
        if (e.error() == QSslError::HostNameMismatch) {
            ignorable.append(e);
        } else {
            qWarning() << "Unacceptable SSL error:" << e.errorString();
        }
    }

    if (ignorable.size() == errors.size()) {
        m_socket->ignoreSslErrors(ignorable);
    } else {
        m_socket->abort();
    }
}

void NetworkLink::processPacket(const Packets::ServerImage &srvImg)
{
    const auto img = QImage::fromData(srvImg.data, "WEBP");

    if (!img.isNull()) {
        [[likely]];
        Q_EMIT imageReady(img);
    }
}

void NetworkLink::processPacket(const Packets::ServerBrightness &brightness)
{
    Q_UNUSED(brightness);
}
