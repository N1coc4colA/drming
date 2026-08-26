#include "server.h"

#include <QSslConfiguration>

#include "parameters.h"

#include "../net/certificatesupport.h"

Server::Server(QObject *parent)
    : QObject{parent}
{}

bool Server::loadServerCertsConfig(QSslConfiguration &outConfig, const QString &protocol)
{
    return ::loadServerCertsConfig(outConfig, protocol, Parameters::instance.serverCertPath, Parameters::instance.serverKeyPath);
}
