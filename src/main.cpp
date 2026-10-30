#include <QCoreApplication>
#include <QCommandLineParser>
#include <QTimer>

#include "core/command.h"
#include "build/buildcommand.h"

Command *command(int argc, char *argv[])
{
    const QString commandName = (argc > 1 ? QString::fromLatin1(argv[1]) : QString());

    if (commandName == QStringLiteral("build"))
        return Command::create<BuildCommand>();

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
    {
        cmd->init(parser);

        QStringList args = app.arguments();
        args.replace(0, app.applicationFilePath());
        parser.process(args);
    }

    QTimer::singleShot(0, &app, [&app, &cmd, &parser] {
        app.exit(cmd->run(parser));
        delete cmd;
        cmd = nullptr;
    });

    return app.exec();
}