#include "packagestep.h"

#include <utils/locator.h>

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
    p->setProgram(Locator::findExecutable(program));
    p->setArguments(arguments);
    p->setProcessChannelMode(QProcess::ForwardedChannels);

    if (!workingDir.isEmpty())
        p->setWorkingDirectory(workingDir);

    if (!environment.isEmpty())
        p->setProcessEnvironment(environment);

    p->start();
    p->waitForFinished(-1);
    p->deleteLater();

    return p;
}
