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
         {{"pv", "video-port"}, QObject::tr("Port on which to expose the video."), QObject::tr("video-port", "Port on which to expose the audio"), "80"},

         {{"i", "ip"}, QObject::tr("IP address on which to expose the service."), QObject::tr("ip-address"), "0.0.0.0"},
         {{"ia", "ip-audio"}, QObject::tr("IP address on which to expose the audio service."), QObject::tr("ip-address-audio"), "239.255.1.1"},
         {{"iv", "ip-video"}, QObject::tr("IP address on which to expose the video service."), QObject::tr("ip-address-video"), "239.255.1.2"},
         {{"if", "iface"}, QObject::tr("Network interface on which to expose the service."), QObject::tr("net-iface"), ""},

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


bool validateIpProto(const QHostAddress &addr)
{
    const auto proto = addr.protocol();

    if (proto == QAbstractSocket::IPv4Protocol ||
        proto == QAbstractSocket::IPv6Protocol ||
        proto == QAbstractSocket::AnyIPProtocol) {
        return true;
    }

    return false;
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
    const auto videoPortName = m_parser.value("video-port");

    const auto serviceIp = QHostAddress(m_parser.value("ip"));
    const auto serviceAudioIp = QHostAddress(m_parser.value("ip-audio"));
    const auto serviceVideoIp = QHostAddress(m_parser.value("ip-video"));
    const auto serviceIface = QNetworkInterface::interfaceFromName(m_parser.value("iface"));

    const auto compressionLevel = m_parser.value("quality");
    const auto streamFormat = m_parser.value("format");

    int port = 0, audioPort = 0, videoPort = 0;

    if (validatePort(portName, port) != Continue) {
        return Failure;
    }
    if (validatePort(audioPortName, audioPort) != Continue) {
        return Failure;
    }
    if (validatePort(videoPortName, videoPort) != Continue) {
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

    if (!serviceIface.isValid() && !m_parser.value("iface").isEmpty()) {
        qCritical() << "The IPv4/IPv6 interface is invalid:" << m_parser.value("iface");
        return Failure;
    }

    if (serviceIp.isNull() || !validateIpProto(serviceIp)) {
        qCritical() << "The argument ip must be a valid IPv4/IPv6 address.";
        return Failure;
    }

    if (serviceAudioIp.isNull() || !serviceAudioIp.isMulticast() || !validateIpProto(serviceAudioIp)) {
        qCritical() << "The argument ip-audio needs to be a multicast IPv6/IPv4 address, and be set." << validateIpProto(serviceAudioIp);
        return Failure;
    }

    if (serviceVideoIp.isNull() || !serviceVideoIp.isMulticast() || !validateIpProto(serviceVideoIp)) {
        qCritical() << "The argument ip-video needs to be a multicast IPv6/IPv4 address, and be set.";
        return Failure;
    }

    Parameters::instance = Parameters{
        .targetScreen = targetScreen,
        .serviceName = serviceName,

        .serviceIp = serviceIp,
        .serviceAudioIp = serviceAudioIp,
        .serviceVideoIp = serviceVideoIp,
        .serviceIface = serviceIface,

        .port = port,
        .audioPort = audioPort,
        .videoPort = videoPort,

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
