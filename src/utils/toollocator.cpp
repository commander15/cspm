#include "toollocator.h"

#include <QStringList>
#include <QStandardPaths>

QString ToolLocator::findGit()
{
    QStringList paths;

    return findExecutable("git", paths);
}

QString ToolLocator::findCmake()
{
    QStringList paths;

    return findExecutable("cmake", paths);
}

QString ToolLocator::findNinja()
{
    QStringList paths;

    return findExecutable("ninja", paths);
}

QString ToolLocator::findExecutable(const QString &name)
{
    return findExecutable(name, QStringList());
}

QString ToolLocator::findExecutable(const QString &name, const QStringList &paths)
{
    // Search on provided paths first
    QString executable = QStandardPaths::findExecutable(name, paths);

    // Later, try using default paths
    if (executable.isEmpty())
        executable = QStandardPaths::findExecutable(name);

    // As last ressort, return as is
    return name;
}
