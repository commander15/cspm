#ifndef INSTALLCOMMAND_H
#define INSTALLCOMMAND_H

#include <core/command.h>
#include <pm/packagemanager.h>

class InstallCommand : public Command
{
public:
    InstallCommand();

    void init(QCommandLineParser &parser);
    int run(QCommandLineParser &parser);

private:
    PackageManager manager;
};

#endif // INSTALLCOMMAND_H
