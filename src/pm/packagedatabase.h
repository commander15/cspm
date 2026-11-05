#ifndef PACKAGEDATABASE_H
#define PACKAGEDATABASE_H

#include <pm/package.h>

class QSqlRecord;

class PackageDatabase
{
public:
    struct PackageItem {
        Package package;
        QList<PackageVersion> versions;
    };

    struct SearchOptions {
        QString query;
        QStringList filters;
        QVariantList params;

        bool withVersions = true;

        int page = 1;
        int itemsPerPage = 50;
    };

    template<typename T>
    struct SearchResult {
        QList<T> data;
        int page = 1;
        int totalPages = 1;

        QString errorCode = 0;
        QString errorString;
    };

    PackageDatabase();

    SearchResult<PackageItem> searchPackages(const SearchOptions &options);

    PackageItem getPackageById(int id);
    PackageItem getPackageByName(const QString &name);
    PackageItem savePackage(const PackageItem &item, bool syncVersions = false);
    bool deletePackageById(int packageId);

    static Package packageFromRecord(const QVariantHash &record);
    static PackageVersion versionFromRecord(const QVariantHash &record);
    static PackageDependency dependencyFromRecord(const QVariantHash &record);

private:
    int upsertPackage(const Package &package);
    int upsertVersion(int packageId, const PackageVersion &version);
    int upsertDependency(int versionId, const PackageDependency &dependency);

    bool deletePackage(int id);
    bool deleteVersion(int id);
    bool deleteDependency(int id);
};

#endif // PACKAGEDATABASE_H
