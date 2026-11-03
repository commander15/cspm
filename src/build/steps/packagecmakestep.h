#ifndef PACKAGECMAKESTEP_H
#define PACKAGECMAKESTEP_H

#include <pm/steps/packagestep.h>

class PackageCMakeStep : public PackageStep
{
public:
    QString name() const override;
    PackageResult run(const PackageContext &context) override;

    PackageResult configure(const PackageContext &context);
    PackageResult build(const QStringList &targets, const PackageContext &context);

    QProcessEnvironment environment(const PackageContext &context);

    static PackageCMakeStep *make(const QString &step, Command *command);
    static PackageCMakeStep *make(const QString &name, const QStringList &targets, Command *command);

private:
    PackageCMakeStep(const QString &step, Command *command);

    const QString m_step;
    QStringList targets;
};

#endif // PACKAGECMAKESTEP_H