#ifndef PACKAGE_BUILDER_H
#define PACKAGE_BUILDER_H

#include <core/command.h>
#include <pm/package.h>

class PackageBuild;
class PackageStep;

class PackageBuilder : public QObject
{
    Q_OBJECT

public:
    struct BuildOptions {
        bool dev = false;
        bool cache = true;
    };

    PackageBuilder(Command *command, QObject *parent = nullptr);
    ~PackageBuilder();

    PackageBuild *build(const Package &package, const PackageVersion &version, const QString &outputDir, const BuildOptions &options);
    PackageBuild *build(const PackageContext &context);

private:
    ConsoleStreams console;
    QList<PackageStep *> steps;

    friend class PackageBuild;
};

class PackageBuild : public QObject
{
    Q_OBJECT

signals:
    void finished(const PackageResult &result);

private:
    PackageBuild(const PackageContext &context, PackageBuilder *builder);

    void start();

    const PackageContext context;
    PackageBuilder *m_builder;

    friend class PackageBuilder;
};

#endif // PACKAGE_BUILDER_H