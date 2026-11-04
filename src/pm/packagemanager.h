#ifndef PACKAGEMANAGER_H
#define PACKAGEMANAGER_H

#include <core/command.h>
#include <pm/steps/packagestep.h>

#include <QObject>
#include <QJsonObject>

class Package;
class PackageVersion;
class PackageManagerSettings;

class PackageManager : public QObject
{
    Q_OBJECT

public:
    struct SearchOptions {
        enum MatchType { ExactMatch, RelativeMatch, StrictMatch };
        MatchType match = RelativeMatch;
    };

    struct InstallOptions {
        bool dev = false;
        bool cache = true;
        bool lowBandwidth = false;
    };

    explicit PackageManager(Command *command, QObject *parent = nullptr);

    QUrl registryUrl() const;
    static QString rootDir();

    bool fetchIndex();
    bool fetchMetas();
    void reloadPackages();

    QList<QPair<Package, QList<PackageVersion>>> search(const QString &query, const SearchOptions &options) const;
    QPair<Package, QList<PackageVersion>> get(const QString &packageName) const;
    QPair<Package, QList<PackageVersion>> get(const QString &packageName, const QString &versionFilter) const;

    QStringList prepareInstall(const QStringList &packageNames) const;

    int install(const QString &packageName, const InstallOptions &options);
    int install(const QString &packageName, const QString &version, const InstallOptions &options);
    int install(const Package &package, const PackageVersion &version, const InstallOptions &options);

    QString packageInstallPrefix(const Package &package, const PackageVersion &version) const;
    QString packageOutputDir(const Package &package) const;

    QString getCMakeUserRegistryPath(const Package &package) const;
    void registerWithCMakeUserRegistry(const Package &package, const PackageVersion &version);
    void unregisterWithCMakeUserRegistry(const Package &package, const PackageVersion &version);

    static bool versionMatch(const QString &version, const QString &filter);

private:
    QUrl url(const QString &path) const;

    QList<Package> getPackages(const QString &filters) const;
    QList<PackageVersion> getPackageVersions(const QString &filters) const;

    ConsoleStreams console;
    Command *command;

    QString m_packagesFile;
    QString m_packagesDir;
    QString m_packagesTarget;
    QString m_packagesTemp;
    QJsonObject m_packagesCache;
};


class PackageManagerSettings
{
public:
};

#endif // PACKAGEMANAGER_H
