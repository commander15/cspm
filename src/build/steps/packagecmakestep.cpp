#include "packagecmakestep.h"

#include <QDir>

PackageCMakeStep::PackageCMakeStep(const QString &step, Command *command)
    : PackageStep(command)
    , m_step(step)
{}

PackageResult PackageCMakeStep::run(const PackageContext &context)
{
    QDir dir(context.buildDir);
    if (!dir.exists())
        dir.mkpath(".");

    if (m_step == "configure")
        return configure(context);

    if (m_step == "custom")
        return build(targets, context);

    auto targets = (context.dev ? context.version.build.dev : context.version.build.runtime).targets;

    if (m_step == "build")
        return build(targets.value("build", { "all" }), context);

    if (m_step == "test" && targets.contains("test"))
        return build(targets.value("test"), context);

    if (m_step == "install")
        return build(targets.value("install", { "install" }), context);

    if (m_step == "package" && targets.contains("package"))
        return build(targets.value("package"), context);

    return PackageResult::make(0);
}

PackageResult PackageCMakeStep::configure(const PackageContext &context)
{
    QStringList args;
    args << "-S" << context.sourceDir << "-B" << context.buildDir;

    auto build = (context.dev ? context.version.build.dev : context.version.build.runtime);

    // 1. Append generator if specified
    if (!build.generator.isEmpty()) {
        args << "-G" << build.generator;
    }

    // 2. Append CMake variables (-DKEY=VALUE)
    auto var = [&args](const QString &name, const QString &value) { args << QString("-D%1=%2").arg(name, value); };
    for (auto it = build.variables.begin(); it != build.variables.end(); ++it) {
        var(it.key(), it.value().toString());
    }

    // 3. Append computed variables
    var("CMAKE_INSTALL_PREFIX", context.installDir);
    var("CPACK_PACKAGE_DIRECTORY", context.packageDir);

    // 4. Append extra arguments
    for (const QString &arg : std::as_const(build.arguments)) {
        args << arg;
    }

    // runProgram handles execution, tool location, and logging automatically
    QProcess *cmake = runProgram("cmake", args, environment(context), context.buildDir);

    PackageResult result = PackageResult::make(cmake->exitCode());
    if (!result.success()) {
        result.message("CMake configuration failed with exit code " + QString::number(result.exitCode()));
    }

    return result;
}

PackageResult PackageCMakeStep::build(const QStringList &targets, const PackageContext &context)
{
    QStringList args;
    args << "--build" << context.buildDir;

    if (!targets.isEmpty())
        args << "--target" << targets;

    // runProgram handles execution, tool location, and logging automatically
    QProcess *cmake = runProgram("cmake", args, environment(context), context.buildDir);

    PackageResult result = PackageResult::make(cmake->exitCode());
    if (!result.success()) {
        result.message("CMake configuration failed with exit code " + QString::number(result.exitCode()));
    }

    return result;
}

PackageCMakeStep *PackageCMakeStep::make(const QString &step, Command *command)
{
    static const QStringList supportedSteps = {
        "configure",
        "build",
        "test",
        "install",
        "package"
    };

    if (supportedSteps.contains(step, Qt::CaseInsensitive))
        return new PackageCMakeStep(step.toLower(), command);
    else
        return new PackageCMakeStep("configure", command);
}

PackageCMakeStep *PackageCMakeStep::make(const QString &name, const QStringList &targets, Command *command)
{
    PackageCMakeStep *step = new PackageCMakeStep("custom", command);
    step->targets = targets;
    return step;
}

QProcessEnvironment PackageCMakeStep::environment(const PackageContext &context)
{
    auto build = (context.dev ? context.version.build.dev : context.version.build.runtime);

    // Set up environment (start with system env, then overlay manifest env)
    QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();

    for (auto it = build.environment.begin(); it != build.environment.end(); ++it) {
        environment.insert(it.key(), it.value().toString());
    }

    return environment;
}