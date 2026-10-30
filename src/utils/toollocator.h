#ifndef TOOLLOCATOR_H
#define TOOLLOCATOR_H

#include <QString>

class ToolLocator
{
public:
    static QString findGit();
    static QString findCmake();
    static QString findNinja();

    static QString findExecutable(const QString &name);
    static QString findExecutable(const QString &name, const QStringList &paths);
};

#endif // TOOLLOCATOR_H
