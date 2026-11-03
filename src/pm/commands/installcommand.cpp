#include "installcommand.h"

#include <QCommandLineParser>

#define INSTALL_DEV           "dev"
#define INSTALL_NO_CACHE      "no-cache"
#define INSTALL_LOW_BANDWIDTH "lowband"

InstallCommand::InstallCommand()
    : manager(this)
{}

void InstallCommand::init(QCommandLineParser &parser)
{
    QCommandLineOption dev(INSTALL_DEV, "install dev package");
    QCommandLineOption noCache(INSTALL_NO_CACHE, "disable caching");
    QCommandLineOption lowBand(INSTALL_LOW_BANDWIDTH, "poor connection");
    parser.addOptions({ dev, noCache, lowBand });

    parser.addPositionalArgument("package", "package name");
}

int InstallCommand::run(QCommandLineParser &parser)
{
    const QStringList packages = manager.prepareInstall(parser.positionalArguments());

    if (packages.isEmpty())
        return 0;

    const PackageManager::InstallOptions options = {
        .dev = parser.isSet(INSTALL_DEV),
        .cache = !parser.isSet(INSTALL_NO_CACHE),
        .lowBandwidth = parser.isSet(INSTALL_LOW_BANDWIDTH)
    };

    for (const QString &package : packages) {
        int result = manager.install(package, options);

        // We add a new line and flush
        console.out << Qt::endl;

        if (result != 0) return result;
    }

    return 0;
}
