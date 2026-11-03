#ifndef PACKAGESTEP_H
#define PACKAGESTEP_H

#include <core/command.h>
#include <pm/package.h>

#include <QString>
#include <QProcess>

class PackageContext;
class PackageResult;

class PackageStep
{
public:
    PackageStep(Command *command);
    virtual ~PackageStep() = default;

    virtual QString name() const { return QStringLiteral("Step"); }
    virtual PackageResult run(const PackageContext &context) = 0;

    template<typename Step>
    Step *make()
    { return new Step(Command::defaultCommand()); }

    template<typename Step>
    Step *make(const QString &name)
    { return Step::make(name, Command::defaultCommand()); }

    template<typename Step>
    static PackageResult runStep(const PackageContext &context)
    {
        QScopedPointer<Step> step(new Step(Command::defaultCommand()));
        return step->run(context);
    }
    template<typename Step>
    static PackageResult runStep(const QString &name, const PackageContext &context)
    {
        QScopedPointer<Step> step(Step::make(name, Command::defaultCommand()));
        return step->run(context);
    }

    static PackageResult runOne(const PackageContext &context, PackageStep *step, const ConsoleStreams &console);
    static PackageResult runOne(const QString &title, const PackageContext &context, PackageStep *step, const ConsoleStreams &console);

    static PackageResult runAll(const PackageContext &context, const QList<PackageStep *> &steps, const ConsoleStreams &console);
    static PackageResult runAll(const QString &title, const PackageContext &context, const QList<PackageStep *> &steps, const ConsoleStreams &console);

protected:
    QProcess *runProgram(const QString &program, const QStringList &arguments, const QString &workingDir = QString());
    QProcess *runProgram(const QString &program, const QStringList &arguments, const QProcessEnvironment &environment, const QString &workingDir = QString());

    ConsoleStreams console;
};

#endif // PACKAGESTEP_H
