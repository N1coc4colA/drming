#include "avahi_publisher.h"
#include "commandparser.h"
#include "displayreader.h"
#include "dispsetup.h"
#include "server.h"

#include <QBuffer>
#include <QCoreApplication>
#include <QTimer>
#include <QtEndian>

#include <iostream>

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    app.setApplicationName("drming");
    app.setApplicationVersion("1.0");

    CommandParser parser{};
    if (!parser.parse()) {
        return EXIT_FAILURE;
    }

    if (parser.parser().isSet("help")) {
        parser.parser().showHelp(EXIT_SUCCESS);
    }

    if (parser.parser().isSet("version")) {
        parser.parser().showVersion();
        return EXIT_SUCCESS;
    }

    const QString targetScreen = parser.parser().value("display");
    const QString serviceName = parser.parser().value("service");
    const QString portName = parser.parser().value("port");
    const QString serviceIp = parser.parser().value("ip");
    const auto serviceHostIp = serviceIp.isEmpty() ? QHostAddress::Any : QHostAddress(serviceIp);
    bool portIsValid = false;

    const int port = portName.toInt(&portIsValid);
    if (!portIsValid) {
        std::cerr << "It seems the port is not base 10, and could not be parsed.\n";
        return EXIT_FAILURE;
    }

    if (port < 1 || port > 65535) {
        std::cerr << qPrintable(QObject::tr("Port number is not within the right range.")) << '\n';
        return EXIT_FAILURE;
    }

    if (serviceHostIp.isNull()) {
        std::cerr << "The supplied service exposure IP is invalid.\n";
        return EXIT_FAILURE;
    }

    auto publisher = new AvahiPublisher(serviceName, "_drming._tcp", static_cast<uint16_t>(port));
    QObject::connect(&app, &QCoreApplication::aboutToQuit, publisher, &AvahiPublisher::stop);
    QObject::connect(publisher, &AvahiPublisher::started, []() { std::cout << "Service publishing started.\n"; });
    QObject::connect(publisher, &AvahiPublisher::stopped, []() { std::cout << "Service publishing stopped.\n"; });

    if (!parser.parser().isSet("no-advertise")) {
        publisher->start();
    }

    DispSetup setup(targetScreen);
    if (!setup.isSetup()) {
        return EXIT_FAILURE;
    }
    std::cout << "Setup display on connector " << qPrintable(setup.virtualConnectorName()) << '\n';

    Server server{};
    if (!server.listen(serviceHostIp, port)) {
        return EXIT_FAILURE;
    }

    DisplayReader reader(setup.virtualConnectorName());

    QTimer timer{};
    timer.setInterval(60);

    QObject::connect(&server, &Server::clientConnected, &timer, qOverload<>(&QTimer::start));
    QObject::connect(&server, &Server::noClient, &timer, &QTimer::stop);
    QObject::connect(&timer, &QTimer::timeout, [&reader, &server]() {
        VkmsFrameBuffer fb{};
        if (!reader.getVkmsFrameBuffer(fb)) {
            return;
        }

        QByteArray pixelBuffer(static_cast<const char *>(fb.data), static_cast<int>(fb.stride * fb.height));
        QByteArray headerBuffer{};
        {
            QDataStream stream(&headerBuffer, QIODevice::WriteOnly);
            stream.setByteOrder(QDataStream::BigEndian);
            stream << static_cast<quint16>(qtImageFormatFromDrm(fb.format));
            stream << static_cast<quint32>(fb.width);
            stream << static_cast<quint32>(fb.height);
            stream << static_cast<quint32>(fb.stride);
        }

        reader.releaseVkmsFrameBuffer(fb);

        server.broadcast(headerBuffer);
        server.broadcast(pixelBuffer);
    });

    std::cout << "Ready!\n";

    if (server.hasClient()) {
        timer.start();
    }

    return app.exec();
}
