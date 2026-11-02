#ifndef BUILDCOMMAND_H
#define BUILDCOMMAND_H

#include "packagebuilder.h"

#include <core/command.h>

class BuildCommand : public Command
{
public:
    struct Options {
        QString outputDir;
        bool dev = true;
        bool cache = true;
    };

    BuildCommand();

    void init(QCommandLineParser &parser) override;
    int run(QCommandLineParser &parser) override;

    int buildOne(const QString &metaFile, const QString &versionFile, const Options &options);
    int buildAll(const QString &indexFile, const QString &packagesDir, const Options &options);

private:
    PackageBuilder m_builder;
};

#endif // BUILDCOMMAND_H