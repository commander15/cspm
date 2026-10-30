#ifndef COMMAND_P_H
#define COMMAND_P_H

#include "command.h"

#include <QCommandLineParser>
#include <QStringList>

class DefaultCommand : public Command
{
public:
    void init(QCommandLineParser &parser) override
    {
        parser.addVersionOption();
    }

    int run(QCommandLineParser &parser) override
    {
        static const QStringList commands = { "build" };
        console.out << "available commands: " << commands.join(", ") << Qt::endl;
        return 0;
    }
};

#endif // COMMAND_P_H
