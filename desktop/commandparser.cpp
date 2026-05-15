#include "commandparser.h"

#include <QCoreApplication>
#include <QDebug>

#include <stdlib.h>

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
    });
}

bool CommandParser::parse()
{
    if (!m_parser.parse(qApp->arguments())) {
        qCritical() << m_parser.errorText();
        return false;
    }

    return true;
}
