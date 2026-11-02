#ifndef UPGRADECOMMAND_H
#define UPGRADECOMMAND_H

#include <core/command.h>
#include <pm/packagemanager.h>

class UpgradeCommand : public Command
{
public:
    UpgradeCommand();

    void init(QCommandLineParser &parser);
    int run(QCommandLineParser &parser);

private:
    PackageManager manager;
};

#endif // UPGRADECOMMAND_H
