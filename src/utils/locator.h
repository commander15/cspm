#ifndef LOCATOR_H
#define LOCATOR_H

#include <QString>

class Locator
{
public:
    static QString findGit();
    static QString findCmake();
    static QString findNinja();

    static QString findExecutable(const QString &name);
    static QString findExecutable(const QString &name, const QStringList &paths);
};

#endif // LOCATOR_H
