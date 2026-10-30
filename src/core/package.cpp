#include "package.h"

#include <utils/json.h>

#include <QJsonObject>
#include <QJsonArray>

PackageSource PackageArtifact::toSource() const
{
    return to<PackageSource>();
}

PackageBinary PackageArtifact::toBinary() const
{
    return to<PackageBinary>();
}
void PackageVersion::fill(const QJsonObject &data)
{
    this->dependencies = JSON::transform<PackageDependency>(data.value("dependencies").toObject(), [](const QPair<QString, QJsonValue> &value) {
        auto dep = JSON::make<PackageDependency>(value.second.toObject());
        dep.names.insert("default", value.first);
        return dep;
    });

    this->build = JSON::make<PackageBuildData>(data.value("build").toObject());

    this->artifacts = JSON::transform<PackageArtifact>(data.value("artifacts").toObject(), [](const QPair<QString, QJsonValue> &value) {
        auto artifact = JSON::make<PackageArtifact>(value.second.toObject());
        artifact.name = value.first;
        return artifact;
    });
}

void PackageBuildData::fill(const QJsonObject &data)
{
    auto list = [&data] (const QString &key) {
        const QJsonObject o = data.value(key).toObject();
        return o.toVariantHash();
    };

    this->targets = JSON::transformHash<QString, QStringList>(data.value("targets").toObject(), [](const QPair<QString, QJsonValue> &item) {
        return QPair<QString, QStringList>({ item.first, item.second.toVariant().value<QStringList>() });
    });

    this->variables = list("vars");
    this->arguments = data.value("args").toVariant().value<QStringList>();
    this->environment = list("env");

    this->dependencies = JSON::transform<PackageDependency>(data.value("dependencies").toArray(), [](const QJsonValue &item) {
        return JSON::make<PackageDependency>(item.toObject());
    });
}

void PackageDependency::fill(const QJsonObject &data)
{
    const QJsonValue name = data.value("name");
    if (name.isString())
        names = { { "default", name.toString() } };
    else
        names = JSON::transformHash<QString, QString>(name.toObject(), [] (const QPair<QString, QJsonValue> &value) {
            return QPair<QString, QString>({ value.first, value.second.toString() });
        });
}
