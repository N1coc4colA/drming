#include "avahi_publisher.h"
#include "commandparser.h"
#include "displaymanager.h"
#include "parameters.h"
#include "server.h"

#include "../settings.h"

#include <QCoreApplication>

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName("drming");
    QCoreApplication::setApplicationVersion("1.0");

    switch (CommandParser().parse()) {
    case CommandParser::Failure:
        return EXIT_FAILURE;
    case CommandParser::Stop:
        return EXIT_SUCCESS;
    case CommandParser::Continue:
        break;
    }

    if (!Parameters::instance.advertise) {
        const auto publisher = new AvahiPublisher(Parameters::instance.serviceName,
                                            Settings::advertisementServiceType,
                                            static_cast<uint16_t>(Parameters::instance.port),
                                            &app);
        publisher->start();
    }

    DisplayManager manager{};
    Server server{};

    QObject::connect(&server, &Server::clientConnected, [&manager](NetworkClient *client) {
        // An error occurred.
        if (!manager.registerClient(client)) {
            // If registration failed, drop client
            client->deleteLater();
        }
    });

    if (!server.listen(Parameters::instance.serviceHostIp, Parameters::instance.port)) {
        return EXIT_FAILURE;
    }

    qInfo() << "Ready!";

    return QCoreApplication::exec();
}
