#include <QCoreApplication>
#include <QCommandLineParser>
#include <QTimer>

#include "core/command.h"
#include "build/buildcommand.h"
#include "pm/commands/updatecommand.h"
#include "pm/commands/installcommand.h"
#include "pm/commands/upgradecommand.h"
#include "pm/commands/removecommand.h"

Command *command(int argc, char *argv[])
{
    const QString commandName = (argc > 1 ? QString::fromLatin1(argv[1]) : QString());

    if (commandName == QStringLiteral("build"))
        return Command::create<BuildCommand>();

    if (commandName == QStringLiteral("update"))
        return Command::create<UpdateCommand>();

    if (commandName == QStringLiteral("install"))
        return Command::create<InstallCommand>();

    if (commandName == QStringLiteral("upgrade"))
        return Command::create<UpgradeCommand>();

    if (commandName == QStringLiteral("remove"))
        return Command::create<RemoveCommand>();

    return Command::defaultCommand();
}

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    app.setApplicationName("cspm");
    app.setApplicationVersion(CSPM_VERSION);

    QCommandLineParser parser;
    parser.setApplicationDescription("...");
    parser.addHelpOption();

    Command *cmd = command(argc, argv);
    cmd->init(parser);
    {
        QStringList args = app.arguments();
        if (args.size() > 1)
            args.removeAt(1);
        parser.process(args);
    }

    QTimer::singleShot(0, &app, [&app, &cmd, &parser] {
        app.exit(cmd->run(parser));
        delete cmd;
        cmd = nullptr;
    });

    return app.exec();
}