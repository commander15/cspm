#include "json.h"

#include <nlohmann/json-schema.hpp>
#include <regex>

#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QMetaProperty>

using namespace nlohmann;
using namespace nlohmann::json_schema;

class custom_error_handler : public error_handler
{
public:
    void error(const json::json_pointer &pointer, const json &json, const std::string &msg) override
    { this->hasError = true; this->msg = msg; }

    bool hasError = false;
    std::string msg;
};

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

    return parse(device->readAll(), error);
}

QJsonValue JSON::parse(const QByteArray &json, JSONError *error)
{
    QJsonDocument doc = QJsonDocument::fromJson(json, error == nullptr ? nullptr : &error->parseError);
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

bool JSON::validate(const QString &fileName, const QByteArray &jsonData, QString *message)
{
    QFile f(fileName);
    if (!f.open(QIODevice::ReadOnly)) {
        if (message) {
            *message = f.errorString();
        }
        return false;
    }

    return validateRaw(f.readAll(), jsonData, message);
}

bool JSON::validateRaw(const QByteArray &schemaData, const QByteArray &jsonData, QString *message)
{
    try {
        // 1. Parse JSON data and schema
        json schemaJson = json::parse(schemaData.constData());
        json instanceJson = json::parse(jsonData.constData());

        // 2. Define a custom format checker handler
        auto format_checker = [](const std::string &format, const std::string &value) {
            if (format == "regex") {
                try {
                    // Test if the string is a valid C++ regular expression
                    std::regex pattern(value);
                    return true;
                } catch (...) {
                    return false;
                }
            }
            // For any other unknown formats, return true to avoid blocking validation
            return true;
        };

        // 3. Initialize the validator with our custom format checker
        json_validator validator(nullptr, format_checker);
        validator.set_root_schema(schemaJson);

        // 4. Validate the instance
        custom_error_handler errorHandler;
        validator.validate(instanceJson, errorHandler);

        if (errorHandler.hasError) {
            if (message)
                *message = QString::fromUtf8(errorHandler.msg);
            return false;
        }

        if (message) {
            *message = "Validation successful.";
        }
        return true;

    } catch (const std::exception &e) {
        if (message) {
            *message = QString::fromUtf8(e.what());
        }
        return false;
    } catch (...) {
        if (message) {
            *message = "Unknown error occurred during JSON schema validation.";
        }
        return false;
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
