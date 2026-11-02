#include "locator.h"

#include <QStringList>
#include <QStandardPaths>

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

QString Locator::findNinja()
{
    QStringList paths;

    return findExecutable("ninja", paths);
}

QString Locator::findExecutable(const QString &name)
{
    return findExecutable(name, QStringList());
}

QString Locator::findExecutable(const QString &name, const QStringList &paths)
{
    // Search on provided paths first
    QString executable = QStandardPaths::findExecutable(name, paths);

    // Later, try using default paths
    if (executable.isEmpty())
        executable = QStandardPaths::findExecutable(name);

    // As last ressort, return as is
    if (executable.isEmpty())
        executable = name;

    return executable;
}
