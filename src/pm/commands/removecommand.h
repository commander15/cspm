#ifndef REMOVECOMMAND_H
#define REMOVECOMMAND_H

#include <core/command.h>

class RemoveCommand : public Command
{
public:
    RemoveCommand();

    void init(QCommandLineParser &parser);
    int run(QCommandLineParser &parser);
};

#endif // REMOVECOMMAND_H
