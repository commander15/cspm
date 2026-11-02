#include "packageinstallstep.h"

#include <pm/packagemanager.h>
#include <pm/steps/packagefetchstep.h>

#include <QEventLoop>
#include <QDir>

PackageInstallStep::PackageInstallStep(PackageManager *manager, Command *command)
    : PackageStep(command)
    , manager(manager)
    , builder(command)
    , fetch("binary", command)
{}

PackageResult PackageInstallStep::run(const PackageContext &context)
{
    if (context.dev)
        return buildAndInstall(context);

    const PackageBinary binary = context.version.binary();
    if (!binary.name.isEmpty())
        return decompressAndInstall(context, binary);

    return buildAndInstall(context);
}

PackageResult PackageInstallStep::buildAndInstall(PackageContext context)
{
    QHash<QString, QString> env;

    // CMake prefix path update
    const QStringList cmakePrefixPaths = { };
    if (!cmakePrefixPaths.isEmpty()) {
        if (qEnvironmentVariableIsEmpty("CMAKE_PREFIX_PATH")) {
            env.insert("CMAKE_PREFIX_PATH", cmakePrefixPaths.join(";"));
        } else {
            env.insert("CMAKE_PREFIX_PATH", qEnvironmentVariable("CMAKE_PREFIX_PATH") + ';' + cmakePrefixPaths.join(";"));
        }
    }

    context.environment.insert(env);

    PackageBuild *build = builder.build(context);
    QEventLoop loop;

    QObject::connect(build, &PackageBuild::finished, &loop, [build, &loop](const PackageResult &result) {
        build->deleteLater();
        loop.exit(result.exitCode());
    });

    int result = loop.exec();

    // Register on CMake User Registry
    if (result == 0)
        manager->registerWithCMakeUserRegistry(context.package, context.version);

    return PackageResult::make(result);
}

PackageResult PackageInstallStep::decompressAndInstall(const PackageContext &context, const PackageBinary &binary)
{
    return fetch.run(context);
}

PackageInstallStep *PackageInstallStep::make(PackageManager *manager, Command *command)
{
    return new PackageInstallStep(manager, command);
}