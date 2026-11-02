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
    if (!manager.fetchIndex())
        return 1;

    if (!manager.fetchMetas())
        return 2;

    return 0;
}
