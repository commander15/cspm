#include "packagestep.h"

#include <utils/toollocator.h>

#include <QCoreApplication>
#include <QProcess>
#include <QProcessEnvironment>
#include <QTimer>

PackageStep::PackageStep(Command *command)
    : console(command->console)
{}

QProcess *PackageStep::runProgram(const QString &program, const QStringList &arguments, const QString &workingDir)
{
    return runProgram(program, arguments, QProcessEnvironment(), workingDir);
}

QProcess *PackageStep::runProgram(const QString &program, const QStringList &arguments, const QProcessEnvironment &environment, const QString &workingDir)
{
    QProcess *p = new QProcess(qApp);
    p->setProgram(ToolLocator::findExecutable(program));
    p->setArguments(arguments);

    if (!workingDir.isEmpty())
        p->setWorkingDirectory(workingDir);

    if (!environment.isEmpty())
        p->setProcessEnvironment(environment);

    QObject::connect(p, &QProcess::readyReadStandardOutput, p, [this, p] {
        console.out << p->readAllStandardOutput() << Qt::flush;
    });

    QObject::connect(p, &QProcess::readyReadStandardError, p, [this, p] {
        console.out << p->readAllStandardError() << Qt::flush;
    });

    p->start();
    p->waitForFinished(-1);
    p->deleteLater();

    return p;
}
