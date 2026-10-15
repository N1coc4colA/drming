#include "avahi_publisher.h"
#include "commandparser.h"
#include "displaymanager.h"
#include "parameters.h"
#include "server.h"
#include "streamsocket.h"

#include <QCoreApplication>

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName("drming");
    QCoreApplication::setApplicationVersion("1.0");

    switch (CommandParser().parseApp()) {
    case CommandParser::Failure:
        return EXIT_FAILURE;
    case CommandParser::Stop:
        return EXIT_SUCCESS;
    case CommandParser::Continue:
        break;
    }

    Server server{};
    StreamSocket videoSocket{};
    StreamSocket audioSocket{};

    DisplayManager manager(videoSocket);

    QObject::connect(&server, &Server::clientConnected, [&manager](NetworkClient *client) {
        // An error occurred.
        if (!manager.registerClient(client)) {
            // If registration failed, drop client
            client->deleteLater();
        }
    });
    QObject::connect(&videoSocket, &StreamSocket::keyChanged, [&videoSocket, &audioSocket]() {
        audioSocket.setKey(videoSocket.getKey(), videoSocket.getKeystamp());
    });

    if (!videoSocket.listen(Parameters::instance.serviceVideoIp,
                            Parameters::instance.videoPort,
                            Parameters::instance.serviceIface)) {
        return EXIT_FAILURE;
    }
    if (!audioSocket.listen(Parameters::instance.serviceAudioIp,
                            Parameters::instance.audioPort,
                            Parameters::instance.serviceIface)) {
        return EXIT_FAILURE;
    }
    audioSocket.setKey(videoSocket.getKey(), videoSocket.getKeystamp());

    if (!server.listen(Parameters::instance.serviceIp, Parameters::instance.serviceIp, Parameters::instance.port)) {
        return EXIT_FAILURE;
    }

    if (!Parameters::instance.advertise) {
        const auto publisher = new AvahiPublisher(Parameters::instance.serviceName,
                                                  "_drming._udp",
                                                  static_cast<uint16_t>(Parameters::instance.port),
                                                  &app);
        publisher->start();
    }

    qInfo() << "Ready!";

    return QCoreApplication::exec();
}
