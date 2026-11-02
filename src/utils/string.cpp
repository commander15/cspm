#include "string.h"

#include <QString>
#include <QStringList>

QStringList String::mergeCMakeStringLists(const QStringList &base, const QStringList &other)
{
    QStringList result = base;

    for (const QString &newItem : other) {
        // Extract a key if it's a flag/assignment (e.g., "-DVAR=VAL" -> key is "-DVAR" or "VAR")
        // Here we look for '=' or space as a delimiter, or fall back to exact match.
        QString key = newItem;
        if (newItem.contains('=')) {
            key = newItem.section('=', 0, 0);
        } else if (newItem.contains(' ')) {
            key = newItem.section(' ', 0, 0);
        }

        bool found = false;
        for (int i = 0; i < result.size(); ++i) {
            QString existingItem = result[i];
            QString existingKey = existingItem;
            if (existingItem.contains('=')) {
                existingKey = existingItem.section('=', 0, 0);
            } else if (existingItem.contains(' ')) {
                existingKey = existingItem.section(' ', 0, 0);
            }

            if (existingKey == key) {
                result[i] = newItem; // Override existing
                found = true;
                break;
            }
        }

        if (!found) {
            result.append(newItem); // Append otherwise
        }
    }

    return result;
}
