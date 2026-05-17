#include "avahi_publisher.h"
#include "commandparser.h"
#include "displayreader.h"
#include "dispsetup.h"
#include "parameters.h"
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
    switch (parser.parse()) {
    case CommandParser::Failure:
        return EXIT_FAILURE;
    case CommandParser::Stop:
        return EXIT_SUCCESS;
    case CommandParser::Continue:
        break;
    }

    if (!Parameters::instance.advertise) {
        auto publisher = new AvahiPublisher(Parameters::instance.serviceName, "_drming._tcp", static_cast<uint16_t>(Parameters::instance.port), &app);
        publisher->start();
    }

    DispSetup setup(Parameters::instance.targetScreen);
    if (!setup.isSetup()) {
        return EXIT_FAILURE;
    }
    qInfo() << "Setup display on connector " << setup.virtualConnectorName();

    Server server{};
    if (!server.listen(Parameters::instance.serviceHostIp, Parameters::instance.port)) {
        return EXIT_FAILURE;
    }

    DisplayReader reader(setup.virtualConnectorName());

    QTimer timer{};
    timer.setInterval(60);

    bool primaryFailureNotice = false;

    QObject::connect(&server, &Server::clientConnected, &timer, qOverload<>(&QTimer::start));
    QObject::connect(&server, &Server::noClient, &timer, &QTimer::stop);
    QObject::connect(&timer, &QTimer::timeout, [&reader, &server, &primaryFailureNotice]() {
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
            result.save(&buf, "JPEG", Parameters::instance.jpegCompression);
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
