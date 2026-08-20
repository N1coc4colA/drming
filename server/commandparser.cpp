#include "commandparser.h"

#include <QCoreApplication>
#include <QDebug>
#include <QMetaEnum>

#include <cstdlib>

#include "parameters.h"

template<typename T>
QStringList possibleValues()
{
    const auto me = QMetaEnum::fromType<T>();

    QStringList l;
    l.reserve(me.keyCount());

    for (int i = 0; i < me.keyCount(); i++) {
        l.append(me.key(i));
    }

    return l;
}

CommandParser::CommandParser()
{
    m_parser.setApplicationDescription("Utility to share a new screen over the network.");
    m_parser.addHelpOption();
    m_parser.addVersionOption();
    m_parser.addOptions({
         {{"d", "display"}, QObject::tr("Sets the display's name."), QObject::tr("display", "System name of the display"), "drming"},
         {{"s", "service"}, QObject::tr("Sets the sevices' name when advertising through mDNS."), QObject::tr("service-name"), "DRMing"},
         {{"a", "no-advertise"}, QObject::tr("Sets whether or not to disable advertising on the network using mDNS. An non-null value evaluates to true.")},

         {{"p", "port"}, QObject::tr("Port on which to expose."), QObject::tr("port", "Port on which to expose"), "80"},
         {{"pa", "audio-port"}, QObject::tr("Port on which to expose the audio."), QObject::tr("audio-port", "Port on which to expose the audio"), "80"},

         {{"i4", "ipv4"}, QObject::tr("IP address on which to expose the service for IPv4."), QObject::tr("ip-address-4"), "0.0.0.0"},
         {{"i6", "ipv6"}, QObject::tr("IP address on which to expose the service for IPv6."), QObject::tr("ip-address-6"), "::"},
         {{"ia4", "ipv4-audio"}, QObject::tr("IP address on which to expose the audio service for IPv4."), QObject::tr("ip-address-audio-4"), "239.255.1.1"},
         {{"ia6", "ipv6-audio"}, QObject::tr("IP address on which to expose the audio service for IPv6."), QObject::tr("ip-address-audio-6"), "ff02::1:1"},
         {{"if4", "iface4"}, QObject::tr("Network interface on which to expose the service for IPv4."), QObject::tr("net-iface-4"), ""},
         {{"if6", "iface6"}, QObject::tr("Network interface on which to expose the service for IPv6."), QObject::tr("net-iface-6"), ""},

         {{"q", "quality"}, QObject::tr("Sets the WEBP quality level. 0 to 100, 100 meaning best quality. You can set to -1 to use default quality level."), QObject::tr("quality"), "70"},
         {{"f", "format"}, QObject::tr("Stream format to use. Must be one of: %1.").arg(possibleValues<Opts::DisplayStreamType>().join(", ")), QObject::tr("format"), "h265"},

         {{"k", "key"}, QObject::tr("Key to use for server encryption"), QObject::tr("key", "Key to use for server encryption"), "./certs/server.key"},
         {{"c", "cert"}, QObject::tr("Certificate for server encryption"), QObject::tr("cert", "Certificate for server encryption"), "./certs/server.crt"},
         {{"t", "trusted"}, QObject::tr("Trusted clients' certificates"), QObject::tr("trusted", "Trusted clients' certificates"), "./certs/valids/*"},
    });
}


CommandParser::Exit validatePort(const QString &port, int &out)
{
    bool valid = false;
    out = port.toInt(&valid);
    if (!valid) {
        qCritical() << "It seems the port is not base 10, and could not be parsed.";
        return CommandParser::Failure;
    }
    if (out < 1 || out > 65535) {
        qCritical() << QObject::tr("Port number is not within the right range.");
        return CommandParser::Failure;
    }

    return CommandParser::Continue;
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
    const auto audioPortName = m_parser.value("audio-port");

    const auto serviceIp4 = QHostAddress(m_parser.value("ipv4"));
    const auto serviceIp6 = QHostAddress(m_parser.value("ipv6"));
    const auto serviceAudioIp4 = QHostAddress(m_parser.value("ipv4-audio"));
    const auto serviceAudioIp6 = QHostAddress(m_parser.value("ipv6-audio"));
    const auto serviceAudioIface4 = QNetworkInterface::interfaceFromName(m_parser.value("iface4"));
    const auto serviceAudioIface6 = QNetworkInterface::interfaceFromName(m_parser.value("iface6"));

    const auto compressionLevel = m_parser.value("quality");
    const auto streamFormat = m_parser.value("format");

    int port = 0, audioPort = 0;

    if (validatePort(portName, port) != Continue) {
        return Failure;
    }
    if (validatePort(audioPortName, audioPort) != Continue) {
        return Failure;
    }

    if (!possibleValues<Opts::DisplayStreamType>().contains(streamFormat)) {
        qCritical() << "Invalid stream format:" << streamFormat;
        return Failure;
    }

    bool valid = false;
    const auto quality = compressionLevel.toInt(&valid);
    if (!valid) {
        qCritical() << QObject::tr("The supplied compression level is not base 10, and could not be parsed.");
        return Failure;
    }
    if (quality < -1 || quality > 100) {
        qCritical() << QObject::tr("The compression level is not within the right range.");
        return Failure;
    }

    if (serviceIp4.isNull() && serviceIp6.isNull()) {
        qCritical()  << "You need to at least provide ipv4 or ipv6 arguments to run this program.";
        return Failure;
    }

    if (!serviceIp4.isNull() && serviceAudioIp4.isNull()) {
        qCritical() << "Specifying either ipv4 requires ipv4-audio to be set.";
        return Failure;
    }
    if (!serviceIp6.isNull() && serviceAudioIp6.isNull()) {
        qCritical() << "Specifying either ipv6 requires ipv6-audio to be set.";
        return Failure;
    }

    if (!serviceIp4.isNull() && serviceIp4.protocol() != QAbstractSocket::IPv4Protocol) {
        qCritical() << "The argument ipv4 must be an IPv4 address.";
        return Failure;
    }
    if (serviceAudioIp4.protocol() != QAbstractSocket::IPv4Protocol) {
        qCritical() << "The argument ipv4-audio must be an IPv4 address.";
        return Failure;
    }
    if (!serviceIp6.isNull() && serviceIp6.protocol() != QAbstractSocket::IPv6Protocol) {
        qCritical() << "The argument ipv6 must be an IPv6 address.";
        return Failure;
    }
    if (serviceAudioIp6.protocol() != QAbstractSocket::IPv6Protocol) {
        qCritical() << "The argument ipv6-audio must be an IPv6 address.";
        return Failure;
    }

    if (!serviceAudioIp4.isNull() && !serviceAudioIp4.isMulticast()) {
        qCritical() << "The argument ipv4-audio needs to be a multicast address.";
        return Failure;
    }
    if (!serviceAudioIp6.isNull() && !serviceAudioIp6.isMulticast()) {
        qCritical() << "The argument ipv6-audio needs to be a multicast address.";
        return Failure;
    }

    Parameters::instance = Parameters{
        .targetScreen = targetScreen,
        .serviceName = serviceName,

        .serviceIp4 = serviceIp4,
        .serviceAudioIp4 = serviceAudioIp4,
        .serviceAudioIface4 = serviceAudioIface4,
        .serviceIp6 = serviceIp6,
        .serviceAudioIp6 = serviceAudioIp6,
        .serviceAudioIface6 = serviceAudioIface6,
        .port = port,
        .audioPort = audioPort,

        .qualityLevel = quality,
        .advertise = m_parser.isSet("no-advertise"),

        .trustedCertsPath = m_parser.value("trusted"),
        .serverCertPath = m_parser.value("cert"),
        .serverKeyPath = m_parser.value("key"),
        .streamFormat = static_cast<Opts::DisplayStreamType>(
            QMetaEnum::fromType<Opts::DisplayStreamType>().keyToValue(streamFormat.toLocal8Bit())),
    };

    return Continue;
}
