#include "server.h"

#include <QSslConfiguration>

#include "parameters.h"

#include "../certificatesupport.h"

Server::Server(QObject *parent)
    : QObject{parent}
{}

bool Server::loadServerCertsConfig(QSslConfiguration &outConfig, const QString &protocol)
{
    const auto serverCert = openCertificate(Parameters::instance.serverCertPath);
    const auto serverKey = openKey(Parameters::instance.serverKeyPath);
    if (serverKey.isNull() || serverCert.isNull()) {
        [[unlikely]];

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
