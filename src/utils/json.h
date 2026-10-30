#ifndef JSON_H
#define JSON_H

#include <QString>
#include <QJsonObject>
#include <QJsonArray>

struct JSONError {
    QString errorString() const
    { return (!errorText.isEmpty() ? errorText : parseError.errorString()); }

    bool isValid() const
    { return errorCode != 0; }

    int errorCode = 0;
    QString errorText;
    QJsonParseError parseError;
};

class JSON
{
public:
    template<typename T>
    static T make(const QString &fileName, JSONError *error = nullptr)
    {
        QScopedPointer<JSONError> tmpError;
        if (error == nullptr) {
            tmpError.reset(new JSONError());
            error = tmpError.get();
        }

        QJsonObject jsonObject = parse(fileName, error).toObject();
        return make<T>(jsonObject, error);
    }

    template<typename T>
    static T make(const QJsonObject &jsonObject, JSONError *error = nullptr)
    {
        T o;

        QScopedPointer<JSONError> tmpError;
        if (error == nullptr) {
            tmpError.reset(new JSONError());
            error = tmpError.get();
        }

        if (error->isValid()) return o;

        fill(&o, T::staticMetaObject, jsonObject, error);

        return o;
    }

    template<typename T, typename UnaryOp>
    static QList<T> transform(const QJsonObject &root, UnaryOp &&callback)
    {
        QList<T> list;
        list.reserve(root.size());

        for (auto &&[key, val] : root.asKeyValueRange()) {
            // Normalize Qt 6 view/ref types to standard QString and QJsonValue
            QString k = key.toString();
            QJsonValue v = val;

            // Pass a standard QPair (or std::pair) to the callback
            list.append(callback(qMakePair(k, v)));
        }

        return list;
    }

    template<typename K, typename V, typename UnaryOp>
    static QHash<K, V> transformHash(const QJsonObject &root, UnaryOp &&callback)
    {
        QHash<K, V> hash;
        hash.reserve(root.size());

        for (auto &&[key, val] : root.asKeyValueRange()) {
            // Normalize Qt 6 view/ref types to standard QString and QJsonValue
            QString k = key.toString();
            QJsonValue v = val;

            // Pass a standard QPair (or std::pair) to the callback
            auto pair = callback(qMakePair(k, v));
            hash.insert(pair.first, pair.second);
        }

        return hash;
    }

    template<typename T, typename UnaryOp>
    static QList<T> transform(const QJsonArray &data, UnaryOp &&callback)
    {
        QList<T> list;
        list.reserve(data.size());

        for (auto &&item : data) {
            // Pass a standard QPair (or std::pair) to the callback
            list.append(callback(item));
        }

        return list;
    }

    static QJsonValue parse(const QString &fileName, JSONError *error = nullptr);
    static QJsonValue parse(QIODevice *device, JSONError *error = nullptr);
    static void fill(void *object, const QMetaObject &metaObject, const QJsonObject &data, JSONError *error = nullptr);

    static QString fieldName(const QString &propertyName);

private:
    static void xerror(int code, const QString &text, JSONError *error = nullptr)
    { if (error == nullptr) return; error->errorText = text; }

    static void xerror(int code, const QJsonParseError &parseError, JSONError *error = nullptr)
    { if (error == nullptr) return; error->parseError = parseError; }
};

#endif // JSON_H
