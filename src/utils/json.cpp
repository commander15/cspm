#include "json.h"

#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QMetaProperty>

QJsonValue JSON::parse(const QString &fileName, JSONError *error)
{
    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly)) {
        xerror(1, file.errorString(), error);
        return QJsonObject();
    }

    return parse(&file, error);
}

QJsonValue JSON::parse(QIODevice *device, JSONError *error)
{
    if (!device->isReadable()) {
        xerror(2, (device->errorString().isEmpty() ? "can't json data from the device" : device->errorString()), error);
        return QJsonObject();
    }

    QJsonDocument doc = QJsonDocument::fromJson(device->readAll(), error == nullptr ? nullptr : &error->parseError);
    if (error != nullptr && error->parseError.error != QJsonParseError::NoError) {
        error->errorCode = 3;
        return QJsonObject();
    }

    if (doc.isObject())
        return QJsonValue(doc.object());

    if (doc.isArray())
        return QJsonValue(doc.array());

    return QJsonValue();
}

void JSON::fill(void *object, const QMetaObject &metaObject, const QJsonObject &data, JSONError *error)
{
    // First we detect if we will work on a QObject
    bool isQObject = (metaObject.inherits(&QObject::staticMetaObject));

    // We check for generic flag
    bool generic = false;
    {
        int index = metaObject.indexOfClassInfo("generic");
        if (index >= 0)
            generic = QString::fromLatin1(metaObject.classInfo(index).value()) != "false";
    }

    int filled = 0;
    int total = metaObject.propertyCount();
    for (int i(0); i < total; ++i) {
        const QMetaProperty property = metaObject.property(i);
        const QString fieldName = JSON::fieldName(property.name());
        const bool genericProperty = (generic && fieldName == "data");

        if (data.contains(fieldName) || genericProperty) {
            bool success = false;

            const QVariant value = (genericProperty ? data.toVariantHash() : data.value(fieldName).toVariant());
            if (isQObject)
                success = property.write(static_cast<QObject *>(object), value);
            else
                success = property.writeOnGadget(object, value);

            if (success)
                ++filled;
            else if (error == nullptr)
                qWarning() << "Can't fill property" << property.name();
        } else if (error == nullptr) {
            qWarning() << "Can't fill property" << property.name();
        }
    }

    // We check if the object can handle filling itself
    {
        QMetaMethod fillMethod;
        int index = metaObject.indexOfMethod("fill(QJsonObject)");
        if (index >= 0) {
            fillMethod = metaObject.method(index);
            if (fillMethod.parameterCount() > 0 && fillMethod.parameterType(0) == QMetaType::fromType<QJsonObject>().id()) {
                if (isQObject)
                    fillMethod.invoke(static_cast<QObject *>(object), data);
                else
                    fillMethod.invokeOnGadget(object, data);
            }
        }
    }
}

QString JSON::fieldName(const QString &propertyName)
{
    QString fieldName;
    for (const QChar &c : propertyName) {
        if (c.isUpper())
            fieldName.append('_' + c.toLower());
        else
            fieldName.append(c);
    }
    return fieldName;
}
