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

    PackageFetchStep(const QString &step, Command *command);

    QString name() const override { return QStringLiteral("Fetch"); }
    PackageResult run(const PackageContext &context) override;

    PackageResult clone(const QString &repo, const QString &id, GitIdType type, const QString &dir, bool lowBandwidth);

    PackageResult download(const QUrl &url, const QString &outputDir, bool lowBandwidth);
    PackageResult download(const QUrl &url, const QString &sha, const QString &fileName, bool lowBandwidth);
    PackageResult download(const QUrl &url, const QString &sha, QIODevice *device, bool lowBandwidth);

    PackageResult decompress(const QString &fileName, const QString &outputDir);

    static PackageFetchStep *make(const QString &step, Command *command);

private:
    PackageResult gitFetch(const QString &repo, const QString &id, GitIdType type, const QString &dir, bool lowBandwidth);
    PackageResult cloneFastly(const QString &repo, const QString &id, GitIdType type, const QString &dir);
    PackageResult cloneSlowly(const QString &repo, const QString &id, GitIdType type, const QString &dir);

    const QString m_step;
};

#endif // PACKAGEFETCHSTEP_H