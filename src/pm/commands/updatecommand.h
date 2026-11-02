#ifndef UPDATECOMMAND_H
#define UPDATECOMMAND_H

#include <core/command.h>
#include <pm/packagemanager.h>

class UpdateCommand : public Command
{
public:
    UpdateCommand();

    void init(QCommandLineParser &parser);
    int run(QCommandLineParser &parser);

private:
    PackageManager manager;
};

#endif // UPDATECOMMAND_H
