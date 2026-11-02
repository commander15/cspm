#include "upgradecommand.h"

UpgradeCommand::UpgradeCommand()
    : manager(this)
{}

void UpgradeCommand::init(QCommandLineParser &parser)
{
    //
}

int UpgradeCommand::run(QCommandLineParser &parser)
{
    return 0;
}
