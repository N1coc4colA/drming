#ifndef COMMANDPARSER_H
#define COMMANDPARSER_H

#include <QCommandLineParser>

/// Parses server command-line options into the shared Parameters snapshot.
class CommandParser
{
public:
    /// Action to take after parsing command-line options.
    enum Exit {
        /// Command-line arguments were invalid; exit with an error.
        Failure,
        /// Informational option handled (help/version); exit cleanly.
        Stop,
        /// Arguments were valid; continue application startup.
        Continue,
    };

    CommandParser();

    /// Returns whether startup should fail, stop after help/version, or continue.
    Exit parse(const QStringList &args);
    /// Parses arguments from QCoreApplication::arguments().
    Exit parseApp()
    {
        return parse(qApp->arguments());
    }

private:
    QCommandLineParser m_parser{};
};

#endif // COMMANDPARSER_H
