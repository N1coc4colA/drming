#ifndef COMMANDPARSER_H
#define COMMANDPARSER_H

#include <QCommandLineParser>

/// Parses server command-line options into the shared Parameters snapshot.
class CommandParser
{
public:
    enum Exit {
        Failure,
        Stop,
        Continue,
    };

    CommandParser();

    /// Returns whether startup should fail, stop after help/version, or continue.
    Exit parse(const QStringList &args);
    inline Exit parseApp() {
        return parse(qApp->arguments());
    }

private:
    QCommandLineParser m_parser{};
};

#endif // COMMANDPARSER_H
