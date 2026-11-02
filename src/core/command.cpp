#include "command.h"
#include "command_p.h"

Command::Command()
    : out(stdout)
    , err(stderr)
    , in(stdin)
    , console({ .out = out, .err = err, .in = in })
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
