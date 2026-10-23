#include "avahi_publisher.h"
#include "commandparser.h"
#include "displaymanager.h"
#include "parameters.h"
#include "server.h"
#include "streamsocket.h"

#include <QCoreApplication>
#include <QSocketNotifier>

#include <ctrack.hpp>

#include <csignal>
#include <sys/socket.h>
#include <unistd.h>


namespace {

int sigFd[2] = { -1, -1 };

void requestShutdown(const int sig)
{
    const char byte = 1;
    // write() and signal() are async-signal-safe; qApp->quit() is not.
    ::write(sigFd[0], &byte, sizeof(byte));
    // second Ctrl+C gets the default (hard) action
    std::signal(sig, SIG_DFL);
}

} // namespace

int main(int argc, char *argv[])
{
    // Keep the server headless: all UI-facing work is performed by the client.
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName("drming");
    QCoreApplication::setApplicationVersion("1.0");

    if (::socketpair(AF_UNIX, SOCK_STREAM, 0, sigFd) != 0) {
        return EXIT_FAILURE;
    }

    QSocketNotifier sigNotifier(sigFd[1], QSocketNotifier::Read);
    QObject::connect(&sigNotifier, &QSocketNotifier::activated, [&sigNotifier]() {
        char byte;
        (void)::read(sigFd[1], &byte, sizeof(byte));
        sigNotifier.setEnabled(false);
        QCoreApplication::quit();
    });

    std::signal(SIGINT,  requestShutdown);
    std::signal(SIGTERM, requestShutdown);   // needed by systemd / containers


    switch (CommandParser().parseApp()) {
    case CommandParser::Failure:
        return EXIT_FAILURE;
    case CommandParser::Stop:
        return EXIT_SUCCESS;
    case CommandParser::Continue:
        break;
    }

    // The control server authenticates clients; the two stream sockets carry
    // audio and video and share the video session key.
    Server server{};
    StreamSocket videoSocket{};
    StreamSocket audioSocket{};

    DisplayManager manager(videoSocket);

    QObject::connect(&app, &QCoreApplication::aboutToQuit, &server, &Server::close);
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
        const auto publisher = new AvahiPublisher(
            Parameters::instance.serviceIface,
            Parameters::instance.serviceIp.protocol() == QAbstractSocket::IPv6Protocol,
            Parameters::instance.serviceName,
            "_drming._udp",
            static_cast<uint16_t>(Parameters::instance.port),
            &app
            );
        publisher->start();
    }

    qInfo() << "Ready!";

    const auto ret_code = QCoreApplication::exec();

    ctrack::result_print();

    return ret_code;
}
