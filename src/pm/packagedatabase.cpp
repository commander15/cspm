#include "packagedatabase.h"

#include <utils/db.h>
#include <utils/json.h>

#include <QSqlRecord>

PackageDatabase::PackageDatabase()
{}

PackageDatabase::SearchResult<PackageDatabase::PackageItem> PackageDatabase::searchPackages(const SearchOptions &options)
{
    SearchResult<PackageDatabase::PackageItem> results;

    // Start with SELECT *, letting your domain mappers pull fields directly from the record
    QString statement = "SELECT packages.*";
    if (options.withVersions) {
        // If needed, you can include package_versions.*.
        // Note: If both have 'id', alias package_versions.id as 'version_id' to prevent key collision in QVariantHash.
        statement.append(", package_versions.*");
    }
    statement.append(" FROM packages");

    QVariantList params = options.params;
    QStringList filters = options.filters;

    if (options.withVersions) {
        // Adjust 'versions' table name if your schema uses 'versions' instead of 'package_versions'
        statement.append(" LEFT JOIN versions AS package_versions ON (packages.id = package_versions.package_id)");
    }

    if (!options.query.isEmpty()) {
        filters.append("packages.name LIKE ?");
        params.append("%" + options.query + "%");
    }

    if (!filters.isEmpty()) {
        statement.append(" WHERE " + filters.join(" AND "));
    }

    if (options.withVersions) {
        statement.append(" ORDER BY packages.id, package_versions.id");
    }

    if (options.page > 1 && options.itemsPerPage > 0) {
        int offset = (options.page - 1) * options.itemsPerPage;
        statement.append(QString("LIMIT %1 OFFSET %2").arg(options.itemsPerPage).arg(offset));
    }

    Package lastPackage;
    QList<PackageVersion> versions;

    const QSqlError error = DB::select(statement, params, [&lastPackage, &versions, &results, &options](const QVariantHash &record) {
        // Use your existing record mappers directly!
        Package package = packageFromRecord(record); // or packageFromRecord(record)

        if (!options.withVersions) {
            results.data.append({ package, {} });
            return;
        }

        PackageVersion version = versionFromRecord(record); // or versionFromRecord(record)

        if (lastPackage.id <= 0) {
            lastPackage = package;
            versions.append(version);
        } else if (package.id == lastPackage.id) {
            versions.append(version);
        } else {
            results.data.append({ lastPackage, versions });
            lastPackage = package;
            versions.clear();
            versions.append(version);
        }
    });

    // Flush the final remaining package group
    if (options.withVersions && lastPackage.id > 0) {
        results.data.append({ lastPackage, versions });
    }

    if (error.isValid()) {
        results.errorCode = error.nativeErrorCode();
        results.errorString = error.databaseText();
    }

    return results;
}

PackageDatabase::PackageItem PackageDatabase::getPackageById(int id)
{
    auto results = searchPackages({ .filters = { "packages.id = ?" }, .params = { id }, .itemsPerPage = 1 });
    return (results.data.isEmpty() ? PackageItem() : results.data.constFirst());
}

PackageDatabase::PackageItem PackageDatabase::getPackageByName(const QString &name)
{
    auto results = searchPackages({ .filters = { "packages.name = ?" }, .params = { name }, .itemsPerPage = 1 });
    return (results.data.isEmpty() ? PackageItem() : results.data.constFirst());
}

PackageDatabase::PackageItem PackageDatabase::savePackage(const PackageItem &item, bool syncVersions)
{
    DB::Transaction t;

    PackageItem newItem;
    newItem.package.id = upsertPackage(item.package);
    if (newItem.package.id <= 0) {
        return PackageItem();
    }

    for (const PackageVersion &inputVersion : item.versions) {
        PackageVersion version = inputVersion;
        version.id = upsertVersion(newItem.package.id, inputVersion);
        if (version.id <= 0) {
            return PackageItem();
        }

        for (const PackageDependency &inputDep : inputVersion.dependencies) {
            PackageDependency dep = inputDep;
            dep.id = upsertDependency(version.id, dep);
            if (dep.id <= 0) {
                return PackageItem();
            }

            version.dependencies.append(dep);
        }

        newItem.versions.append(version);
    }

    t.commit();
    return newItem;
}

bool PackageDatabase::deletePackageById(int packageId)
{
    return deletePackage(packageId);
}

Package PackageDatabase::packageFromRecord(const QVariantHash &record)
{
    Package p;
    // Handle table-qualified or standard column lookups depending on query structure
    p.id = record.value("packages.id", record.value("id")).toInt();
    p.name = record.value("packages.name", record.value("name")).toString();
    p.displayName = record.value("packages.display_name", record.value("display_name")).toString();
    p.cmakeName = record.value("packages.cmake_name", record.value("cmake_name")).toString();
    p.description = record.value("packages.description", record.value("description")).toString();
    return p;
}

PackageVersion PackageDatabase::versionFromRecord(const QVariantHash &record)
{
    PackageVersion v;
    v.id = record.value("versions.id", record.value("id")).toInt();
    v.version = record.value("versions.version", record.value("version")).toString();
    //v.manifestJson = record.value("versions.manifest_json", record.value("manifest_json")).toString();

    // Optional convenience parsing if you also want to populate structured fields from the cached JSON manifest
    /*if (!v.manifestJson.isEmpty()) {
        const QJsonObject manifest = JSON::parse(v.manifestJson.toUtf8()).toObject();
        // If JSON::make handles populating fields onto an existing or new object:
        // v = JSON::make<PackageVersion>(manifest);
        // v.manifestJson = manifestJson; // ensure we preserve raw JSON if needed
    }*/

    return v;
}

PackageDependency PackageDatabase::dependencyFromRecord(const QVariantHash &record)
{
    PackageDependency d;
    // Match columns from your 'dependencies' table schema (id, version_id, dep_name, constraint_str)
    d.id = record.value("dependencies.id", record.value("id")).toInt();
    //d.names = record.value("dependencies.dep_name", record.value("dep_name")).toString();
    //d.constraintStr = record.value("dependencies.constraint_str", record.value("constraint_str")).toString();

    return d;
}

int PackageDatabase::upsertPackage(const Package &package)
{
    const QVariantMap data = {
        { "name", package.name },
        { "display_name", package.name },
        { "cmake_name", (package.cmakeName.isEmpty() ? QVariant() : package.cmakeName) },
        { "description", package.description }
    };

    int id = -1;
    QSqlError error;

    if (package.id > 0) {
        id = package.id;
        error = DB::update("packages", data, "id = " + QString::number(id));
    } else {
        error = DB::insert("packages", data, &id);
    }

    return (error.isValid() ? -1 : id);
}

int PackageDatabase::upsertVersion(int packageId, const PackageVersion &version)
{
    int id = -1;

    const QVariantMap data = {
        { "package_id", packageId },
        { "version", version.version },
        { "manifest_json", "" } // ToDo: add a toJson() method on PackageVersion
    };

    const QSqlError error = DB::insert("package_versions", data, &id);
    return (error.isValid() ? -1 : id);
}

int PackageDatabase::upsertDependency(int versionId, const PackageDependency &dependency)
{
    int id = -1;

    const QVariantMap data = {
        { "version_id", versionId },
        { "dep_name", dependency.names.values() },
        { "dep_type", dependency.type }
    };

    const QSqlError error = DB::insert("package_dependencies", data, &id);
    return (error.isValid() ? -1 : id);
}

bool PackageDatabase::deletePackage(int id)
{
    const QSqlError error = DB::deleteData("packages", "id = " + QString::number(id));
    return (error.isValid() ? false : true);
}

bool PackageDatabase::deleteVersion(int id)
{
    const QSqlError error = DB::deleteData("package_versions", "id = " + QString::number(id));
    return (error.isValid() ? false : true);
}

bool PackageDatabase::deleteDependency(int id)
{
    const QSqlError error = DB::deleteData("package_dependencies", "id = " + QString::number(id));
    return (error.isValid() ? false : true);
}
