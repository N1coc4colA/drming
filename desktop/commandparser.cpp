#include "commandparser.h"

#include <QCoreApplication>
#include <QDebug>

#include <stdlib.h>

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
         {{"c", "compression"}, QObject::tr("Sets the JPEG compression level. 0 to 100, 100 meaning best quality. You can use -1 to let Qt use its default quality level."), QObject::tr("compression"), "70"},
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

    const QString targetScreen = m_parser.value("display");
    const QString serviceName = m_parser.value("service");
    const QString portName = m_parser.value("port");
    const QString serviceIp = m_parser.value("ip");
    const QString compressionLevel = m_parser.value("compression");
    const auto serviceHostIp = serviceIp.isEmpty() ? QHostAddress::Any : QHostAddress(serviceIp);
    bool valid = false;

    const int port = portName.toInt(&valid);
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

    const int jpegCompression = compressionLevel.toInt(&valid);
    if (!valid) {
        qCritical() << QObject::tr("The supplied compression level is not base 10, and could not be parsed.");
        return Failure;
    }
    if (jpegCompression < -1 || jpegCompression > 100) {
        qCritical() << QObject::tr("The compression level is not within the right range.");
        return Failure;
    }

    Parameters::instance = Parameters{
        .targetScreen = targetScreen,
        .serviceName = serviceName,
        .serviceIp = serviceIp,
        .serviceHostIp = serviceHostIp,
        .jpegCompression = jpegCompression,
        .port = port,
        .advertise = m_parser.isSet("no-advertise"),
    };

    return Continue;
}
