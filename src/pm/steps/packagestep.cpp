#include "packagestep.h"

#include <utils/locator.h>

#include <QCoreApplication>
#include <QProcess>
#include <QProcessEnvironment>
#include <QTimer>
#include <QElapsedTimer>

PackageStep::PackageStep(Command *command)
    : console(command->console)
{}

PackageResult PackageStep::runOne(const PackageContext &context, PackageStep *step, const ConsoleStreams &console)
{
    return runOne(QString(), context, step, console);
}

PackageResult PackageStep::runOne(const QString &title, const PackageContext &context, PackageStep *step, const ConsoleStreams &console)
{
    PackageResult result = PackageResult::make(0);

    QString stepName = step->name();
    if (stepName.isEmpty()) {
        stepName = "Unnamed Step";
    }

    QString headerTitle = title;
    if (headerTitle.isEmpty()) {
        headerTitle = "Running step: " + stepName + "...";
    }

    console.out << headerTitle << Qt::endl;

    QElapsedTimer stepTimer;
    stepTimer.start();

    result = step->run(context);

    qint64 elapsedMs = stepTimer.elapsed();

    if (!result.success()) {
        console.err << "[-] Error in step [" << stepName << "]: " << result.message() << Qt::endl;
        return result;
    }

    // Print step success and time taken
    console.out << "[OK] (" << (elapsedMs / 1000.0) << "s)" << Qt::endl;
    return result;
}

PackageResult PackageStep::runAll(const PackageContext &context, const QList<PackageStep *> &steps, const ConsoleStreams &console)
{
    return runAll(QString(), context, steps, console);
}

PackageResult PackageStep::runAll(const QString &title, const PackageContext &context, const QList<PackageStep *> &steps, const ConsoleStreams &console)
{
    if (!title.isEmpty())
        console.out << title << Qt::endl;

    if (steps.isEmpty()) {
        console.out << "[OK] No steps to run" << Qt::endl;
        return PackageResult::make(0);
    } else {
        console.out << Qt::flush;
    }

    PackageResult result = PackageResult::make(0);
    int index = 0;
    int total = steps.size();

    QElapsedTimer totalTimer;
    totalTimer.start();

    for (PackageStep *step : steps) {
        if (!step) continue;
        ++index;

        QString stepName = step->name();
        if (stepName.isEmpty()) {
            stepName = "Unnamed Step";
        }

        QString title = "Running step (" + QString::number(index) + " / " + QString::number(total) + "): " + stepName + "...";

        result = runOne(title, context, step, console);
        if (!result.success()) {
            break;
        }

        if (index < total)
            console.out << Qt::endl; // Break line from the active step intro
    }

    return result;
}

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
