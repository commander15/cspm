#include "locator.h"

#include <QStringList>
#include <QStandardPaths>
#include <QCoreApplication>

QString Locator::findGit()
{
    QStringList paths;

    return findExecutable("git", paths);
}

QString Locator::findCmake()
{
    QStringList paths;

    return findExecutable("cmake", paths);
}

QString Locator::findExecutable(const QString &name)
{
    return findExecutable(name, QStringList());
}

QString Locator::findExecutable(const QString &name, const QStringList &paths)
{
    // Search on provided paths first
    QString executable = QStandardPaths::findExecutable(name, paths);

    // Try using current binary dir
    if (executable.isEmpty())
        executable = QStandardPaths::findExecutable(name, { qApp->applicationDirPath() });

    // Later, try using default paths
    if (executable.isEmpty())
        executable = QStandardPaths::findExecutable(name);

    // As last ressort, return as is
    if (executable.isEmpty())
        executable = name;

    return executable;
}
