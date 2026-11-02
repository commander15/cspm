#ifndef PACKAGEFETCHSTEP_H
#define PACKAGEFETCHSTEP_H

#include <pm/steps/packagestep.h>

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

    PackageResult clone(const QString &repo, const QString &id, GitIdType type, const QString &dir, bool lowBandwidth);

private:
    PackageResult gitFetch(const QString &repo, const QString &id, GitIdType type, const QString &dir, bool lowBandwidth);
    PackageResult cloneFastly(const QString &repo, const QString &id, GitIdType type, const QString &dir);
    PackageResult cloneSlowly(const QString &repo, const QString &id, GitIdType type, const QString &dir);
};

#endif // PACKAGEFETCHSTEP_H