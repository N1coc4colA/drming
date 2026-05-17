#ifndef COMMANDPARSER_H
#define COMMANDPARSER_H

#include <QCommandLineParser>

class CommandParser
{
public:
    enum Exit {
        Failure,
        Stop,
        Continue,
    };

    CommandParser();

    Exit parse();

private:
    QCommandLineParser m_parser{};
};

#endif // COMMANDPARSER_H
