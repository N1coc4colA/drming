#include "commandparser.h"

#include <QCoreApplication>
#include <QDebug>

#include <cstdlib>

#include "parameters.h"

CommandParser::CommandParser()
{
    m_parser.setApplicationDescription("Utility to share a new screen over the network.");
    m_parser.addHelpOption();
    m_parser.addVersionOption();
    m_parser.addOptions({
         {{"p", "port"}, QObject::tr("Port on which to expose."), QObject::tr("port", "Port on which to expose"), "80"},
         {{"d", "display"}, QObject::tr("Sets the display's name."), QObject::tr("display", "System name of the display"), "drming"},
         {{"s", "service"}, QObject::tr("Sets the sevices' name when advertising through mDNS."), QObject::tr("service-name"), "DRMing"},
         {{"a", "no-advertise"}, QObject::tr("Sets whether or not to disable advertising on the network using mDNS. An non-null value evaluates to true.")},
         {{"i", "ip"}, QObject::tr("IP address on which to expose the service."), QObject::tr("ip-address"), ""},
         {{"q", "quality"}, QObject::tr("Sets the WEBP quality level. 0 to 100, 100 meaning best quality. You can set to -1 to use default quality level."), QObject::tr("quality"), "70"},
         {{"k", "key"}, QObject::tr("Key to use for server encryption"), QObject::tr("key", "Key to use for server encryption"), "./certs/server.key"},
         {{"c", "cert"}, QObject::tr("Certificate for server encryption"), QObject::tr("cert", "Certificate for server encryption"), "./certs/server.crt"},
         {{"t", "trusted"}, QObject::tr("Trusted clients' certificates"), QObject::tr("trusted", "Trusted clients' certificates"), "./certs/valids/*"},
    });
}

CommandParser::Exit CommandParser::parse()
{
    if (!m_parser.parse(qApp->arguments())) {
        qCritical() << m_parser.errorText();
        return Failure;
    }

    if (m_parser.isSet("help")) {
        m_parser.showHelp(EXIT_SUCCESS);
    }

    if (m_parser.isSet("version")) {
        m_parser.showVersion();
        return Stop;
    }

    const auto targetScreen = m_parser.value("display");
    const auto serviceName = m_parser.value("service");
    const auto portName = m_parser.value("port");
    const auto serviceIp = m_parser.value("ip");
    const auto compressionLevel = m_parser.value("quality");
    const auto serviceHostIp = serviceIp.isEmpty() ? QHostAddress::Any : QHostAddress(serviceIp);
    auto valid = false;

    const auto port = portName.toInt(&valid);
    if (!valid) {
        qCritical() << "It seems the port is not base 10, and could not be parsed.";
        return Failure;
    }
    if (port < 1 || port > 65535) {
        qCritical() << QObject::tr("Port number is not within the right range.");
        return Failure;
    }

    if (serviceHostIp.isNull()) {
        qCritical() << "The supplied service exposure IP is invalid.";
        return Failure;
    }

    const auto quality = compressionLevel.toInt(&valid);
    if (!valid) {
        qCritical() << QObject::tr("The supplied compression level is not base 10, and could not be parsed.");
        return Failure;
    }
    if (quality < -1 || quality > 100) {
        qCritical() << QObject::tr("The compression level is not within the right range.");
        return Failure;
    }

    Parameters::instance = Parameters{
        .targetScreen = targetScreen,
        .serviceName = serviceName,
        .serviceIp = serviceIp,
        .serviceHostIp = serviceHostIp,
        .qualityLevel = quality,
        .port = port,
        .advertise = m_parser.isSet("no-advertise"),
        .trustedCertsPath = m_parser.value("trusted"),
        .serverCertPath = m_parser.value("cert"),
        .serverKeyPath = m_parser.value("key"),
    };

    return Continue;
}
