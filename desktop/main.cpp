#include "avahi_publisher.h"
#include "commandparser.h"
#include "displayreader.h"
#include "dispsetup.h"
#include "server.h"

#include <QBuffer>
#include <QCoreApplication>
#include <QTimer>
#include <QtEndian>

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
    const QString compressionLevel = parser.parser().value("compression");
    const auto serviceHostIp = serviceIp.isEmpty() ? QHostAddress::Any : QHostAddress(serviceIp);
    bool valid = false;

    const int port = portName.toInt(&valid);
    if (!valid) {
        qCritical() << "It seems the port is not base 10, and could not be parsed.";
        return EXIT_FAILURE;
    }
    if (port < 1 || port > 65535) {
        qCritical() << QObject::tr("Port number is not within the right range.");
        return EXIT_FAILURE;
    }

    if (serviceHostIp.isNull()) {
        qCritical() << "The supplied service exposure IP is invalid.";
        return EXIT_FAILURE;
    }

    const int jpegCompression = compressionLevel.toInt(&valid);
    if (!valid) {
        qCritical() << QObject::tr("The supplied compression level is not base 10, and could not be parsed.");
        return EXIT_FAILURE;
    }
    if (jpegCompression < -1 || jpegCompression > 100) {
        qCritical() << QObject::tr("The compression level is not within the right range.");
        return EXIT_FAILURE;
    }

    auto publisher = new AvahiPublisher(serviceName, "_drming._tcp", static_cast<uint16_t>(port));
    QObject::connect(&app, &QCoreApplication::aboutToQuit, publisher, &AvahiPublisher::stop);
    QObject::connect(publisher, &AvahiPublisher::started, []() { qInfo() << "Service publishing started."; });
    QObject::connect(publisher, &AvahiPublisher::stopped, []() { qInfo() << "Service publishing stopped."; });

    if (!parser.parser().isSet("no-advertise")) {
        publisher->start();
    }

    DispSetup setup(targetScreen);
    if (!setup.isSetup()) {
        return EXIT_FAILURE;
    }
    qInfo() << "Setup display on connector " << setup.virtualConnectorName();

    Server server{};
    if (!server.listen(serviceHostIp, port)) {
        return EXIT_FAILURE;
    }

    DisplayReader reader(setup.virtualConnectorName());

    QTimer timer{};
    timer.setInterval(60);

    bool primaryFailureNotice = false;

    QObject::connect(&server, &Server::clientConnected, &timer, qOverload<>(&QTimer::start));
    QObject::connect(&server, &Server::noClient, &timer, &QTimer::stop);
    QObject::connect(&timer, &QTimer::timeout, [&reader, &server, &primaryFailureNotice, jpegCompression]() {
        VkmsFrameBuffer fb{};
        if (!reader.getVkmsFrameBuffer(fb)) {
            if (!primaryFailureNotice) {
                primaryFailureNotice = true;
                qCritical() << "Failed to get primary";
            }

            return;
        }
        if (primaryFailureNotice) {
            primaryFailureNotice = false;
        }

        CursorFrameBuffer cursorFb{};
        const bool hasCursor = reader.getCursorFrameBuffer(cursorFb, fb);

        QByteArray output{};
        QImage result = reader.imageFromFrameBuffer(static_cast<const uint8_t *>(fb.data), fb.width, fb.height, fb.stride, fb.format);
        if (hasCursor) {
            result = reader.compositeWithCursor(result, cursorFb);
        }

        {
            QBuffer buf(&output);
            buf.open(QIODevice::WriteOnly);
            // high quality = 90, trade CPU vs size
            result.save(&buf, "JPEG", jpegCompression);
            buf.close();

            const qsizetype size = qToBigEndian(static_cast<qsizetype>(output.size()));
            output.prepend(reinterpret_cast<const char *>(&size), sizeof(size));
        }

        reader.releaseVkmsFrameBuffer(fb);

        server.broadcast(output);
    });

    qInfo() << "Ready!";

    if (server.hasClient()) {
        timer.start();
    }

    return app.exec();
}
