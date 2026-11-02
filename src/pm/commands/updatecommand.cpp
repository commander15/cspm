#include "updatecommand.h"

UpdateCommand::UpdateCommand()
    : manager(this)
{}

void UpdateCommand::init(QCommandLineParser &parser)
{
    //
}

int UpdateCommand::run(QCommandLineParser &parser)
{
    return (manager.fetchIndex() ? 0 : -1);
}
