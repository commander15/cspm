#include "packagebuilder.h"

#include "steps/packagecmakestep.h"

#include <pm/steps/packagefetchstep.h>

#include <QTimer>

PackageBuilder::PackageBuilder(Command *command, QObject *parent)
    : QObject{parent}
    , console(command->console)
{
    steps.append(PackageFetchStep::make("sources", command));
    steps.append(PackageCMakeStep::make("configure", command));
    steps.append(PackageCMakeStep::make("build", command));
    steps.append(PackageCMakeStep::make("test", command));
    steps.append(PackageCMakeStep::make("install", command));
    steps.append(PackageCMakeStep::make("package", command));
}

PackageBuilder::~PackageBuilder()
{
    for (PackageStep *step : std::as_const(steps))
        delete step;
}

PackageBuild *PackageBuilder::build(const Package &package, const PackageVersion &version, const QString &outputDir, const BuildOptions &options)
{
    const PackageContext context = {
        .package = package,
        .version = version,
        .sourceDir = outputDir + "/source",
        .buildDir = outputDir + "/build",
        .installDir = outputDir + "/install",
        .packageDir = outputDir + "/packages",
        .dev = options.dev,
        .cache = options.cache
    };

    return build(context);
}

PackageBuild *PackageBuilder::build(const PackageContext &context)
{
    PackageBuild *build = new PackageBuild(context, this);
    QTimer::singleShot(0, build, [build] { build->start(); });
    return build;
}

PackageBuild::PackageBuild(const PackageContext &context, PackageBuilder *builder)
    : QObject{builder}
    , context(context)
    , m_builder(builder)
{
}

void PackageBuild::start()
{
    // ToDo: add env vars, args and vars from cspm.conf
    //context.version.build.variables.insert("CMAKE_PREFIX_PATH", "/opt/Qt/6.10.2/gcc_64");

    PackageResult result;

    for (PackageStep *step : std::as_const(m_builder->steps)) {
        result = step->run(context);
        if (!result.success())
            break;
    }

    emit finished(result);
}
