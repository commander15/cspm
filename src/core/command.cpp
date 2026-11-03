#include "command.h"
#include "command_p.h"

Command::Command()
    : console(ConsoleStreams::std())
{
}

Command::~Command()
{
}

void Command::init(QCommandLineParser &parser)
{
}

Command *Command::defaultCommand()
{
    static DefaultCommandProvider instance;
    return instance.get();
}

void Command::exit(bool exitCode)
{
    m_loop.exit(exitCode);
}

int Command::exec()
{
    return m_loop.exec();
}
