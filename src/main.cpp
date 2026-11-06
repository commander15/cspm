#include <QCoreApplication>
#include <QCommandLineParser>
#include <QTimer>

#include <QEloquent/connection.h>

#include "core/command.h"
#include "build/buildcommand.h"
#include "utils/db.h"

#include "pm/packagemanager.h"
#include "pm/commands/updatecommand.h"
#include "pm/commands/installcommand.h"
#include "pm/commands/upgradecommand.h"
#include "pm/commands/removecommand.h"

bool initDatabase(const QString &path);

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

    if (!initDatabase(PackageManager::rootDir() + "/database.db")) {
        return 1;
    }

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

bool initDatabase(const QString &path)
{
    QEloquent::Connection con = QEloquent::Connection::addConnection(QSqlDatabase::defaultConnectionName(), "QSQLITE", path);

    if (!con.open()) {
        //ConsoleStreams::std().out << db.lastError().;
        return false;
    }

    return DB::runMigrations();
}