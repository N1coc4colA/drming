#ifndef COMMANDPARSER_H
#define COMMANDPARSER_H

#include <QCommandLineParser>

class CommandParser
{
public:
    CommandParser();

    bool parse();
    inline QCommandLineParser &parser() { return m_parser; }

private:
    QCommandLineParser m_parser{};
};

#endif // COMMANDPARSER_H
