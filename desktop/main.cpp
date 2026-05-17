#include "avahi_publisher.h"
#include "commandparser.h"
#include "displaymanager.h"
#include "parameters.h"
#include "server.h"

#include <QCoreApplication>

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

    DisplayManager manager{};
    Server server{};

    QObject::connect(&server, &Server::clientConnected, &manager, &DisplayManager::registerClient);

    if (!server.listen(Parameters::instance.serviceHostIp, Parameters::instance.port)) {
        return EXIT_FAILURE;
    }

    qInfo() << "Ready!";

    return app.exec();
}
