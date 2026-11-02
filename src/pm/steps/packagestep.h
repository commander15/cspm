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

    virtual PackageResult run(const PackageContext &context) = 0;

protected:
    QProcess *runProgram(const QString &program, const QStringList &arguments, const QString &workingDir = QString());
    QProcess *runProgram(const QString &program, const QStringList &arguments, const QProcessEnvironment &environment, const QString &workingDir = QString());

    ConsoleStreams console;
};

#endif // PACKAGESTEP_H
