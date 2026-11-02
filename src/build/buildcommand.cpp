#include "buildcommand.h"

#include <utils/json.h>

#include <QCommandLineParser>
#include <QCommandLineOption>
#include <QDir>

#define BUILD_INDEX    "index"
#define BUILD_PACKAGE  "package"
#define BUILD_VERSION  "version"
#define BUILD_OUTPUT   "output"
#define BUILD_DEV      "dev"
#define BUILD_NO_CACHE "no-cache"

BuildCommand::BuildCommand()
    : m_builder(this)
{}

void BuildCommand::init(QCommandLineParser &parser)
{
    QCommandLineOption index({"i", BUILD_INDEX}, "index json file", "index");
    QCommandLineOption package({"p", BUILD_PACKAGE}, "package json file", "package", "meta.json");
    QCommandLineOption version({"v", BUILD_VERSION}, "package version json file", "version", "version.json");
    QCommandLineOption output({"o", BUILD_OUTPUT}, "output dir", "out-dir", ".");
    QCommandLineOption dev(BUILD_DEV, "build dev package");
    QCommandLineOption noCache(BUILD_NO_CACHE, "disable build cache");
    parser.addOptions({ index, package, version, output, dev, noCache });
}

int BuildCommand::run(QCommandLineParser &parser)
{
    const QDir outputDir(parser.value(BUILD_OUTPUT), ".");

    const Options options = {
        .outputDir = outputDir.absolutePath(),
        .dev = parser.isSet(BUILD_DEV),
        .cache = !parser.isSet(BUILD_NO_CACHE)
    };

    if (parser.isSet(BUILD_INDEX)) {
        const QFileInfo info(parser.value(BUILD_INDEX));
        return buildAll(info.absoluteFilePath(), info.absoluteDir().absolutePath() + "/packages", options);
    }

    return buildOne(parser.value(BUILD_PACKAGE), parser.value(BUILD_VERSION), options);
}

int BuildCommand::buildOne(const QString &metaFile, const QString &versionFile, const Options &options)
{
    Package package = JSON::make<Package>(metaFile);
    PackageVersion version = JSON::make<PackageVersion>(versionFile);

    PackageBuild *build = m_builder.build(package, version, options.outputDir, { .dev = options.dev, .cache = options.cache });
    QObject::connect(build, &PackageBuild::finished, build, [build, this](const PackageResult &result) {
        build->deleteLater();
        exit(result.exitCode());
    });

    return exec();
}

int BuildCommand::buildAll(const QString &indexFile, const QString &packagesDir, const Options &options)
{
    int finished = 0;
    int total = 0;

    auto jsonFile = [&packagesDir](const QString &fileName, const QString &package) {
        return packagesDir + '/' + package + '/' + fileName + ".json";
    };

    QJsonArray packages;
    {
        const QJsonValue value = JSON::parse(indexFile);
        if (value.isArray()) {
            packages = value.toArray();
        } else {
            const QJsonObject object = value.toObject().value("packages").toObject();
            const QStringList keys = object.keys();

            for (const QString &key : keys) {
                QJsonObject p = object.value(key).toObject();
                p.insert("name", key);
                packages.append(p);
            }
        }
    }

    for (const QJsonValue &item : std::as_const(packages)) {
        const QJsonObject object = item.toObject();
        const QString name = object.value("name").toString();
        const QString versionString = object.value(object.contains("version") ? "version" : "latest").toString();

        const Package package = JSON::make<Package>(jsonFile("meta", name));
        const PackageVersion version = JSON::make<PackageVersion>(jsonFile(versionString, name));

        PackageBuild *build = m_builder.build(package, version, options.outputDir + '/' + name, { .dev = options.dev, .cache = options.cache });
        QObject::connect(build, &PackageBuild::finished, build, [build, &finished, &total, this](const PackageResult &result) {
            build->deleteLater();
            ++finished;

            if (finished == total)
                exit(result.exitCode());
        });

        ++total;
    }

    return exec();
}