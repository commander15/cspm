#ifndef PACKAGEINSTALLSTEP_H
#define PACKAGEINSTALLSTEP_H

#include <pm/steps/packagestep.h>
#include <pm/steps/packagefetchstep.h>
#include <build/packagebuilder.h>

class PackageInstallStep : public PackageStep
{
public:
    PackageInstallStep(class PackageManager *manager, Command *command);

    PackageResult run(const PackageContext &context) override;

    PackageResult buildAndInstall(PackageContext context);
    PackageResult decompressAndInstall(const PackageContext &context, const PackageBinary &binary);

    static PackageInstallStep *make(class PackageManager *manager, Command *command);

private:
    PackageManager *manager;
    PackageBuilder builder;

    PackageFetchStep fetch;
};

#endif // PACKAGEINSTALLSTEP_H
