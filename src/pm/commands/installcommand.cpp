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
    const QStringList packages = parser.positionalArguments();

    if (packages.isEmpty())
        return 0;

    const PackageManager::InstallOptions options = {
        .dev = parser.isSet(INSTALL_DEV),
        .cache = !parser.isSet(INSTALL_NO_CACHE),
        .lowBandwidth = parser.isSet(INSTALL_LOW_BANDWIDTH)
    };

    for (const QString &package : packages) {
        QString name;
        QString version;
        int result = 0;

        if (package.contains('@')) {
            int index = package.indexOf('@');
            name = package.mid(0, index);
            version = package.mid(index + 1); // Skip the '@' character!
        } else {
            name = package;
        }

        if (version.isEmpty())
            result = manager.install(name, options);
        else
            result = manager.install(name, version, options);

        if (result != 0) return result;
    }

    return 0;
}
