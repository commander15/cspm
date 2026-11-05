#include "packagemanager.h"

#include <utils/http.h>
#include <utils/json.h>
#include <utils/db.h>

#include <pm/steps/packagefetchstep.h>
#include <pm/steps/packageinstallstep.h>

#ifdef Q_OS_WIN
#   include <windows.h>
#endif

#include <QDir>
#include <QFile>
#include <QQueue>

PackageManager::PackageManager(Command *command, QObject *parent)
    : QObject{parent}
    , console(command->console)
    , command(command)
{
    auto mkDir = [](const QString &dir) {
        QDir d(dir);
        if (!d.exists())
            d.mkpath(".");
    };

    const QString home = rootDir();

    m_packagesFile = home + "/registry/index.json";

    m_packagesDir = home + "/registry/packages";
    mkDir(m_packagesDir);

    m_packagesTarget = home + "/packages";
    mkDir(m_packagesTarget);

    m_packagesTemp = QDir::tempPath() + "/cspm/packages";
    mkDir(m_packagesTemp);

    reloadPackages();
}

QUrl PackageManager::registryUrl() const
{
    // Checks CSPM_REGISTRY_URL in the environment; falls back to the hardcoded CDN if empty
    QString customUrl = qEnvironmentVariable("CSPM_REGISTRY_URL");
    if (!customUrl.isEmpty()) {
        return QUrl(customUrl);
    }

#ifdef CSPM_REGISTRY_URL
    return QUrl(CSPM_REGISTRY_URL);
#else
    return QUrl("https://cdn.jsdelivr.net/gh/commander15/commander-registry@main");
#endif
}

QString PackageManager::rootDir()
{
    QDir dir(QDir::homePath() + "/.cspm");
    if (!dir.exists() && !dir.mkpath("."))
        return QString();
    return dir.absolutePath();
}

bool PackageManager::fetchIndex()
{
    const HttpResponse response = HTTP::get(url("/index.json"), console);

    if (!response.success()) {
        console.err << response.errorString << Qt::endl;
        return false;
    }

    QString message;
    if (!JSON::validate((":/pm/json/schemas/registry_index.json"), response.data, &message)) {
        console.err << message << Qt::endl;
        return false;
    }

    JSONError error;
    QJsonObject index = JSON::parse(response.data, &error).toObject();
    if (error.isValid()) {
        console.err << error.errorString() << Qt::endl;
        return false;
    }

    const QJsonObject packages = index.value("packages").toObject();
    const QStringList packageNames = packages.keys();
    for (const QString &packageName : packageNames) {
        const QJsonObject packageObject = packages.value(packageName).toObject();

        int packageId = -1;
        DB::select("id FROM packages WHERE name = ?", { packageName }, [&packageId](const QVariantHash &record) {
            packageId = record.value("id").toInt();
        });

        QVariantMap data;
        data.insert("name", packageName);
        data.insert("display_name", packageObject.value("display_name"));
        data.insert("description", packageObject.value("description"));

        if (data.contains("cmake_name"))
            data.insert("cmake_name", packageObject.value("cmake_name"));

        if (packageId == -1) {
            DB::insert("packages", data, &packageId);
        } else {
            DB::update("packages", data, "id = " + QString::number(packageId));
        }
    }

    QFile file(m_packagesFile);
    if (!file.open(QIODevice::WriteOnly)) {
        console.err << file.errorString() << Qt::endl;
        return false;
    }

    file.write(response.data);
    if (!file.flush()) {
        console.err << file.errorString() << Qt::endl;
        return false;
    }
    file.close();

    reloadPackages();
    return true;
}

bool PackageManager::fetchMetas()
{
    reloadPackages();

    QStringList names = m_packagesCache.keys();
    names.sort();

    for (const QString &packageName : std::as_const(names)) {
        const QString packageRemotePath = "/packages/" + packageName;
        const QString packageLocalPath = m_packagesDir + '/' + packageName;
        QDir(packageLocalPath).mkpath(".");

        const HttpResponse response = HTTP::get(url(packageRemotePath + "/meta.json"), console);
        if (!response.success()) {
            console.err << response.errorString << Qt::endl;
            continue;
        }

        QString msg;
        if (!JSON::validate(":/pm/json/schemas/package_meta.json", response.data, &msg)) {
            console.err << "invalid meta file for package " << packageName << ": " << msg << Qt::endl;
            continue;
        }

        QFile file(packageLocalPath + "/meta.json");
        if (!file.open(QIODevice::WriteOnly)) {
            continue;
        }

        file.write(response.data);
        file.flush();
        file.close();

        const QStringList versions = m_packagesCache.value(packageName).toObject().value("versions").toVariant().toStringList();
        for (const QString &versionStr : versions) {
            const QStringList remoteNames = { 'v' + versionStr, versionStr };
            HttpResponse response;

            for (const QString &remoteName : remoteNames) {
                response = HTTP::get(url(packageRemotePath + '/' + remoteName + ".json"), console);
                if (response.success())
                    break;
            }

            if (!response.success()) {
                continue;
            }

            file.setFileName(packageLocalPath + '/' + versionStr + ".json");
            if (!file.open(QIODevice::WriteOnly)) {
                continue;
            }

            file.write(response.data);
            file.flush();
            file.close();
        }
    }

    reloadPackages();
    return true;
}

void PackageManager::reloadPackages()
{
    const QJsonObject registry = JSON::parse(m_packagesFile).toObject();
    m_packagesCache = registry.value("packages").toObject();
}

QList<QPair<Package, QList<PackageVersion>>> PackageManager::search(const QString &query, const SearchOptions &options) const
{
    QList<QPair<Package, QList<PackageVersion>>> results;

    // QSqlQuery sql = DB::run("SELECT * FROM packages LEFT JOIN versions ON (versions.package_id = packages.id) WHERE packages.name LIKE '%?%'", { query }, console);
    //

    QStringList keys = m_packagesCache.keys();
    keys.sort();

    for (const QString &packageName : std::as_const(keys)) {
        if (options.match == SearchOptions::ExactMatch && packageName.compare(query, Qt::CaseInsensitive) != 0)
            continue;

        if (options.match == SearchOptions::RelativeMatch && !packageName.contains(query, Qt::CaseInsensitive))
            continue;

        if (options.match == SearchOptions::StrictMatch && packageName.compare(query, Qt::CaseSensitive) != 0)
            continue;

        auto match = get(packageName);
        results.append(match);
    }

    return results;
}

QPair<Package, QList<PackageVersion> > PackageManager::get(const QString &packageName) const
{
    if (!packageName.contains('@'))
        return get(packageName, QString());

    const QString name = packageName.section('@', 0, 0);
    const QString version = packageName.section('@', -1, -1);
    return get(name, version);
}

QPair<Package, QList<PackageVersion> > PackageManager::get(const QString &packageName, const QString &versionFilter) const
{
    if (!m_packagesCache.contains(packageName))
        return {};

    const QJsonObject object = m_packagesCache.value(packageName).toObject();

    Package package = JSON::make<Package>(object);
    package.name = packageName;

    QList<PackageVersion> versions;
    {
        const QString packageDir = m_packagesDir + '/' + packageName;

        const QStringList versionStrings = object.value("versions").toVariant().toStringList();
        for (const QString &versionStr : versionStrings) {
            if (!versionFilter.isEmpty() && !versionMatch(versionStr, versionFilter)) {
                continue;
            }

            const PackageVersion version = JSON::make<PackageVersion>(packageDir + '/' + versionStr + ".json");
            versions.append(version);
        }
    }

    return { package, versions };
}

QStringList PackageManager::prepareInstall(const QStringList &packageNames) const
{
    // Helper structure for parsed package requests
    struct PackageRequest {
        QString name;
        QString versionFilter;
    };

    // 1. Parse input package strings (handling scoped packages like "@scope/pkg@1.0.0")
    QList<PackageRequest> initialRequests;
    for (const QString &item : packageNames) {
        int atIdx = -1;
        if (item.startsWith('@')) {
            atIdx = item.indexOf('@', 1);
        } else {
            atIdx = item.indexOf('@');
        }

        PackageRequest req;
        if (atIdx != -1) {
            req.name = item.left(atIdx);
            req.versionFilter = item.mid(atIdx + 1).trimmed();
        } else {
            req.name = item;
            req.versionFilter = QString();
        }
        initialRequests.append(req);
    }

    // Graph structures
    // Key: "packageName@version" (unique node per package version)
    QMap<QString, PackageVersion> nodeVersions;
    // Adjacency list: node -> set of its direct dependencies ("name@version")
    QMap<QString, QSet<QString>> adjList;
    // In-degree map for topological sorting (Kahn's algorithm)
    QMap<QString, int> inDegree;

    // Helper lambda to compare versions for sorting / selecting latest
    auto compareVersions = [](const QString &v1, const QString &v2) -> int {
        QStringList parts1 = v1.split('.');
        QStringList parts2 = v2.split('.');
        int maxLen = qMax(parts1.size(), parts2.size());
        for (int i = 0; i < maxLen; ++i) {
            int n1 = i < parts1.size() ? parts1[i].toInt() : 0;
            int n2 = i < parts2.size() ? parts2[i].toInt() : 0;
            if (n1 != n2) return n1 - n2;
        }
        return 0;
    };

    // Queue for BFS graph discovery
    QQueue<PackageRequest> queue;
    for (const auto &req : initialRequests) {
        queue.enqueue(req);
    }

    QSet<QString> visitedRequests; // Prevents redundant queue processing for exact same requests

    // 2. Build the dependency graph recursively
    while (!queue.isEmpty()) {
        PackageRequest req = queue.dequeue();

        QString requestKey = req.name + "@" + req.versionFilter;
        if (visitedRequests.contains(requestKey)) {
            continue;
        }
        visitedRequests.insert(requestKey);

        // Fetch package metadata and matching versions
        QPair<Package, QList<PackageVersion>> pkgData = get(req.name, req.versionFilter);
        const QList<PackageVersion> &availableVersions = pkgData.second;
        if (availableVersions.isEmpty()) {
            continue; // Skip if no matching version found
        }

        // If a specific version filter was provided, match it. Otherwise, pick the latest available version.
        PackageVersion targetVersion;
        if (!req.versionFilter.isEmpty()) {
            // Find the version that matches the filter (if multiple, pick the highest matching)
            QList<PackageVersion> matching;
            for (const auto &v : availableVersions) {
                if (versionMatch(v.version, req.versionFilter)) {
                    matching.append(v);
                }
            }
            if (matching.isEmpty()) continue;

            // Sort matching versions ascending to pick the latest
            std::sort(matching.begin(), matching.end(), [compareVersions](const PackageVersion &a, const PackageVersion &b) {
                return compareVersions(a.version, b.version) < 0;
            });
            targetVersion = matching.last(); // Pick highest matching version
        } else {
            // No filter: pick the absolute latest version available
            QList<PackageVersion> sorted = availableVersions;
            std::sort(sorted.begin(), sorted.end(), [compareVersions](const PackageVersion &a, const PackageVersion &b) {
                return compareVersions(a.version, b.version) < 0;
            });
            targetVersion = sorted.last();
        }

        QString nodeKey = req.name + "@" + targetVersion.version;
        if (!nodeVersions.contains(nodeKey)) {
            nodeVersions[nodeKey] = targetVersion;
            if (!inDegree.contains(nodeKey)) {
                inDegree[nodeKey] = 0;
            }
        }

        // Process dependencies of this version
        for (const PackageDependency &dep : targetVersion.dependencies) {
            // Resolve dependency version constraint (e.g., dep.name, dep.versionConstraint)
            QString depNodeKeySearch = dep.name() + "@" + dep.version;

            // Queue the dependency to be resolved and added to the graph
            queue.enqueue({dep.name(), dep.version});

            // Find the matching dependency version object to get its exact version string
            QPair<Package, QList<PackageVersion>> depPkgData = get(dep.name(), dep.version);
            if (!depPkgData.second.isEmpty()) {
                QList<PackageVersion> depVersions = depPkgData.second;
                std::sort(depVersions.begin(), depVersions.end(), [compareVersions](const PackageVersion &a, const PackageVersion &b) {
                    return compareVersions(a.version, b.version) < 0;
                });
                PackageVersion depTargetVersion = depVersions.last(); // Latest satisfying constraint
                QString depNodeKey = dep.name() + "@" + depTargetVersion.version;

                if (!nodeVersions.contains(depNodeKey)) {
                    nodeVersions[depNodeKey] = depTargetVersion;
                    if (!inDegree.contains(depNodeKey)) {
                        inDegree[depNodeKey] = 0;
                    }
                }

                // Edge direction: Dependency must be installed BEFORE Dependent.
                // In Kahn's algorithm or dependency sorting: Dependents point to their dependencies,
                // meaning dependency has in-degree incremented by dependents, OR we reverse order.
                // Standard convention: Dependent -> Dependency means Dependency has incoming edge from Dependent.
                if (!adjList[nodeKey].contains(depNodeKey)) {
                    adjList[nodeKey].insert(depNodeKey);
                    inDegree[depNodeKey]++;
                }
            }
        }
    }

    // 3. Perform Topological Sort (Kahn's Algorithm) with version tie-breaking (oldest first, latest last)
    QStringList ordered;
    QList<QString> zeroInDegreeQueue;

    for (auto it = inDegree.begin(); it != inDegree.end(); ++it) {
        if (it.value() == 0) {
            zeroInDegreeQueue.append(it.key());
        }
    }

    while (!zeroInDegreeQueue.isEmpty()) {
        // Sort zero-in-degree nodes such that if same package name has multiple versions,
        // older version is processed/installed before newer version ("latest must be installed last").
        std::sort(zeroInDegreeQueue.begin(), zeroInDegreeQueue.end(), [compareVersions](const QString &a, const QString &b) {
            QStringList partsA = a.split('@');
            QStringList partsB = b.split('@');
            QString nameA = partsA.first();
            QString nameB = partsB.first();
            if (nameA == nameB) {
                return compareVersions(partsA.last(), partsB.last()) < 0; // Older first
            }
            return nameA < nameB;
        });

        QString current = zeroInDegreeQueue.takeFirst();
        ordered.append(current);

        // Decrease in-degree for neighbors
        for (const QString &neighbor : adjList[current]) {
            inDegree[neighbor]--;
            if (inDegree[neighbor] == 0) {
                zeroInDegreeQueue.append(neighbor);
            }
        }
    }

    // If topological sort output doesn't match all nodes (due to circular dependency),
    // append remaining nodes safely as fallback.
    if (ordered.size() < nodeVersions.size()) {
        for (auto it = nodeVersions.begin(); it != nodeVersions.end(); ++it) {
            if (!ordered.contains(it.key())) {
                ordered.append(it.key());
            }
        }
    }

    std::reverse(ordered.begin(), ordered.end());
    return ordered;
}

int PackageManager::install(const QString &packageName, const InstallOptions &options)
{
    auto match = get(packageName);

    if (match.second.isEmpty()) {
        console.err << "package " << packageName << " not found" << Qt::endl;
        return -1;
    }

    // Install latest version
    return install(match.first, match.second.first(), options);
}

int PackageManager::install(const QString &packageName, const QString &version, const InstallOptions &options)
{
    auto match = get(packageName);

    if (match.second.isEmpty()) {
        console.err << "package " << packageName << " not found" << Qt::endl;
        return -1;
    }

    for (const PackageVersion &ver : std::as_const(match.second))
        if (ver.version == version)
            return install(match.first, ver, options);

    console.err << "Version " << version << " not found for package " << packageName << Qt::endl;
    return -1;
}

int PackageManager::install(const Package &package, const PackageVersion &version, const InstallOptions &options)
{
    const QString tempDir = m_packagesTemp + '/' + package.name + '/' + version.version;

    PackageContext context = {
        .package = package,
        .version = version,
        .sourceDir = tempDir + "/source",
        .buildDir = tempDir + "/build",
        .installDir = packageInstallPrefix(package, version),
        .packageDir = packageOutputDir(package),
        .dev = options.dev,
        .cache = options.cache,
        .lowBandwidth = options.lowBandwidth
    };

    for (const PackageDependency &dep : version.dependencies) {
        int result = 0;

        if (dep.version.isEmpty()) {
            result = PackageManager::install(dep.name(), options);
        } else {
            auto match = get(dep.name(), dep.version);
            if (match.second.isEmpty())
                result = PackageManager::install(dep.name(), options);
            else
                result = PackageManager::install(match.first, match.second.first(), options);
        }

        if (result != 0) return result;
    }

    PackageInstallStep install(this, command);
    return PackageStep::runOne("Installing " + package.name + '@' + version.version, context, &install, console).exitCode();
}

QString PackageManager::packageInstallPrefix(const Package &package, const PackageVersion &version) const
{
    return m_packagesTarget + '/' + package.name + '/' + version.version;
}

QString PackageManager::packageOutputDir(const Package &package) const
{
    return m_packagesTarget + '/' + package.name + "/packages";
}

QString PackageManager::getCMakeUserRegistryPath(const Package &package) const
{
#ifdef Q_OS_LINUX
    return QDir::homePath() + "/.cmake/packages/" + package.cmakeName;
#else
    return QStringLiteral("Software\\Kitware\\CMake\\Packages\\") + package.cmakeName;
#endif
}

void PackageManager::registerWithCMakeUserRegistry(const Package &package, const PackageVersion &version)
{
    const QString versionEntry = version.version + "_cspm";

#ifdef Q_OS_LINUX
    // 1. Get the CMake registry path for this package
    QString registryPath = getCMakeUserRegistryPath(package);

    QDir dir(registryPath);
    if (!dir.exists()) {
        dir.mkpath(".");
    }

    // 2. The install prefix path for this specific version
    QString installPrefix = packageInstallPrefix(package, version);

    // 3. Write the tracking file
    QString filePath = registryPath + '/' + versionEntry;
    QFile file(filePath);
    if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream out(&file);
        out << installPrefix << "\n";
        file.close();
    }
#elif defined(Q_OS_WIN)
    // 1. Convert Qt strings cleanly to Windows native wide strings (std::wstring)
    std::wstring subKey = getCMakeUserRegistryPath(package).toStdWString();
    std::wstring entryName = versionEntry.toStdWString();
    std::wstring installPrefix = packageInstallPrefix(package, version).toStdWString();

    HKEY hKey;
    DWORD disposition;

    // 2. Create or open the registry key
    LONG result = RegCreateKeyExW(
        HKEY_CURRENT_USER,       // Hive (User Package Registry is always HKCU)
        subKey.c_str(),          // Subkey path (Software\Kitware\CMake\Packages\<PackageName>)
        0,                       // Reserved
        NULL,                    // Class type (optional)
        REG_OPTION_NON_VOLATILE, // Persists after reboots
        KEY_SET_VALUE,           // Required security access mask
        NULL,                    // Security attributes
        &hKey,                   // Pointer to the opened key handle
        &disposition             // Returns whether key was opened or created
        );

    if (result != ERROR_SUCCESS) {
        return; // Function is void, just return
    }

    // 3. Set the value using byte size calculation
    DWORD dataSizeInBytes = static_cast<DWORD>((installPrefix.length() + 1) * sizeof(wchar_t));

    result = RegSetValueExW(
        hKey,
        entryName.c_str(),       // The version string serves as the unique entry name
        0,                       // Reserved
        REG_SZ,                  // Data type (Null-terminated string)
        reinterpret_cast<const BYTE*>(installPrefix.c_str()),
        dataSizeInBytes
        );

    // 4. Always close handles
    RegCloseKey(hKey);
#endif
}


void PackageManager::unregisterWithCMakeUserRegistry(const Package &package, const PackageVersion &version)
{
    const QString versionEntry = version.version + "_cspm";

#ifdef Q_OS_LINUX
    // 1. Get the CMake registry path for this package
    QString registryPath = getCMakeUserRegistryPath(package);
    QString filePath = registryPath + '/' + versionEntry;

    QFile file(filePath);
    if (file.exists()) {
        QString installPrefix = packageInstallPrefix(package, version);

        if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QTextStream in(&file);
            QString storedPath = in.readLine().trimmed();
            file.close();

            if (storedPath == installPrefix) {
                file.remove();
            }
        }
    }

    // 2. Clean up the registry folder if empty
    QDir dir(registryPath);
    if (dir.exists() && dir.entryList(QDir::NoDotAndDotDot | QDir::Files | QDir::Dirs).isEmpty()) {
        dir.rmdir(".");
    }
#elif defined(Q_OS_WIN)
    std::wstring subKey = getCMakeUserRegistryPath(package).toStdWString();
    std::wstring entryName = versionEntry.toStdWString();

    HKEY hKey;
    // Open the key with write and query permissions (needed to inspect if it's empty later)
    LONG result = RegOpenKeyExW(
        HKEY_CURRENT_USER,
        subKey.c_str(),
        0,
        KEY_SET_VALUE | KEY_QUERY_VALUE,
        &hKey
        );

    if (result != ERROR_SUCCESS) {
        // If the key doesn't exist, there is nothing to unregister
        return;
    }

    // 1. Delete the specific version value
    RegDeleteValueW(hKey, entryName.c_str());

    // 2. Query the key to see if any values or subkeys remain
    DWORD cSubKeys = 0;
    DWORD cValues = 0;
    result = RegQueryInfoKeyW(
        hKey,                  // Key handle
        NULL, NULL, NULL,      // Class info out parameters
        &cSubKeys,             // Number of subkeys
        NULL, NULL,            // Max subkey sizes
        &cValues,              // Number of values remaining under this key
        NULL, NULL, NULL, NULL // Max value sizes & security descriptor
        );

    // Close the key handle before attempting a parental deletion
    RegCloseKey(hKey);

    // 3. Clean up the parent Registry Key if no values or subkeys remain
    if (result == ERROR_SUCCESS && cSubKeys == 0 && cValues == 0) {
        RegDeleteKeyW(HKEY_CURRENT_USER, subKey.c_str());
    }
#endif
}

bool PackageManager::versionMatch(const QString &version, const QString &filter)
{
    // 1. Clean and trim the filter string
    QString cleanedFilter = filter.trimmed();
    if (cleanedFilter.isEmpty()) {
        return true; // No filter means everything matches
    }

    // 2. Parse operator and target version from the filter
    QString op = "=";
    QString targetStr = cleanedFilter;

    if (cleanedFilter.startsWith(">=")) {
        op = ">=";
        targetStr = cleanedFilter.mid(2).trimmed();
    } else if (cleanedFilter.startsWith("<=")) {
        op = "<=";
        targetStr = cleanedFilter.mid(2).trimmed();
    } else if (cleanedFilter.startsWith(">")) {
        op = ">";
        targetStr = cleanedFilter.mid(1).trimmed();
    } else if (cleanedFilter.startsWith("<")) {
        op = "<";
        targetStr = cleanedFilter.mid(1).trimmed();
    } else if (cleanedFilter.startsWith("=")) {
        op = "=";
        targetStr = cleanedFilter.mid(1).trimmed();
    }

    // 3. Helper lambda to strip leading 'v'/'V' and parse into numeric components
    auto parseVersionParts = [](const QString &vStr) -> QVector<int> {
        QString s = vStr.trimmed();
        if (s.startsWith('v') || s.startsWith('V')) {
            s.remove(0, 1);
        }

        QStringList parts = s.split('.');
        QVector<int> numericParts;
        numericParts.reserve(parts.size());
        for (const QString &part : parts) {
            bool ok = false;
            int val = part.toInt(&ok);
            numericParts.append(ok ? val : 0);
        }
        return numericParts;
    };

    QVector<int> v1Parts = parseVersionParts(version);
    QVector<int> v2Parts = parseVersionParts(targetStr);

    // 4. Compare version parts with zero-padding for differing lengths (e.g., "1.0" vs "1.0.0")
    int maxLen = qMax(v1Parts.size(), v2Parts.size());
    int cmp = 0;

    for (int i = 0; i < maxLen; ++i) {
        int n1 = i < v1Parts.size() ? v1Parts[i] : 0;
        int n2 = i < v2Parts.size() ? v2Parts[i] : 0;

        if (n1 < n2) {
            cmp = -1;
            break;
        } else if (n1 > n2) {
            cmp = 1;
            break;
        }
    }

    // 5. Evaluate based on the operator
    if (op == ">=") return (cmp >= 0);
    if (op == "<=") return (cmp <= 0);
    if (op == ">")  return (cmp > 0);
    if (op == "<")  return (cmp < 0);
    if (op == "=")  return (cmp == 0);

    return false;
}

QUrl PackageManager::url(const QString &path) const
{
    QUrl url = registryUrl();
    url.setPath(url.path() + path);
    return url;
}