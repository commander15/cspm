#ifndef STRING_H
#define STRING_H

#include <QString>
#include <QStringList>

class String
{
public:
    static QStringList mergeCMakeStringLists(const QStringList &base, const QStringList &other);
};

#endif // STRING_H
