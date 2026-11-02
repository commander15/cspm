#ifndef PACKAGEFETCHSTEP_H
#define PACKAGEFETCHSTEP_H

#include <core/packagestep.h>

#include <QString>

class PackageVersion;

class PackageFetchStep : public PackageStep
{
public:
    enum GitIdType {
        GitBranch,
        GitTag,
        GitCommit
    };

    PackageFetchStep(Command *command);

    PackageResult run(const PackageContext &context) override;

    PackageResult clone(const QString &repo, const QString &id, GitIdType type, const QString &dir);
};

#endif // PACKAGEFETCHSTEP_H
