#include "package.h"

#include <utils/json.h>
#include <utils/string.h>

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

void PackageArtifact::fill(const QJsonObject &data)
{
    this->data.remove("type");
}

void PackageVersion::fill(const QJsonObject &data)
{
    this->dependencies = JSON::transform<PackageDependency>(data.value("dependencies").toObject(), [](const QPair<QString, QJsonValue> &value) {
        auto dep = JSON::make<PackageDependency>(value.second.toObject());
        dep.names.insert("default", value.first);
        return dep;
    });

    const QJsonObject buildObject = data.value("build").toObject();
    if (buildObject.contains("runtime") || buildObject.contains("dev")) {
        const PackageBuildData base = JSON::make<PackageBuildData>(buildObject);
        auto buildProfiles = JSON::transformHash<QString, PackageBuildData>(buildObject, [](const QPair<QString, QJsonValue> &item) {
            return QPair<QString, PackageBuildData>({ item.first, JSON::make<PackageBuildData>(item.second.toObject()) });
        });
        this->build.runtime = PackageBuildData::merge(base, buildProfiles.value("runtime", buildProfiles.value("dev")));
        this->build.dev = PackageBuildData::merge(base, buildProfiles.value("dev", buildProfiles.value("runtime")));
    } else {
        this->build.runtime = JSON::make<PackageBuildData>(buildObject);
        this->build.dev = this->build.runtime; // Safe fallback
    }

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

PackageBuildData PackageBuildData::merge(const PackageBuildData &base, const PackageBuildData &other)
{
    PackageBuildData merged;

    // 1. Simple fields: override only if 'other' has a value
    merged.generator = other.generator.isEmpty() ? base.generator : other.generator;

    // 2. List fields: arguments (follows list rules)
    merged.arguments = String::mergeCMakeStringLists(base.arguments, other.arguments);

    // 3. Hash fields: variables (override existing, add others)
    merged.variables = base.variables;
    for (auto it = other.variables.cbegin(); it != other.variables.cend(); ++it) {
        merged.variables.insert(it.key(), it.value());
    }

    // 4. Hash fields: environment (override existing, add others)
    merged.environment = base.environment;
    for (auto it = other.environment.cbegin(); it != other.environment.cend(); ++it) {
        merged.environment.insert(it.key(), it.value());
    }

    // 5. Targets: QHash<QString, QStringList> (hash rules + list rules for values)
    merged.targets = base.targets;
    for (auto it = other.targets.cbegin(); it != other.targets.cend(); ++it) {
        const QString &targetName = it.key();
        const QStringList &otherList = it.value();

        if (merged.targets.contains(targetName)) {
            // Target exists, merge its string list using list rules
            merged.targets[targetName] = String::mergeCMakeStringLists(merged.targets[targetName], otherList);
        } else {
            // New target, add it
            merged.targets.insert(targetName, otherList);
        }
    }

    // 6. Dependencies list (you can apply list rules or direct append/override as needed)
    merged.dependencies = base.dependencies;
    for (const auto &dep : other.dependencies) {
        // Optional: avoid duplicate dependency names if needed
        bool exists = false;
        for (int i = 0; i < merged.dependencies.size(); ++i) {
            if (merged.dependencies[i].name() == dep.name()) {
                merged.dependencies[i] = dep; // override
                exists = true;
                break;
            }
        }
        if (!exists) {
            merged.dependencies.append(dep);
        }
    }

    return merged;
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

PackageSource PackageVersion::source() const
{
    auto it = std::find_if(artifacts.begin(), artifacts.end(), [](const PackageArtifact &artifact) {
        return artifact.name == "source" || artifact.type == "git";
    });

    return (it == artifacts.end() ? PackageSource() : it->toSource());
}
void Package::fill(const QJsonObject &data)
{
    if (cmakeName.isEmpty())
        cmakeName = data.value("display_name").toString();

    if (cmakeName.isEmpty())
        cmakeName = data.value("name").toString();
}
