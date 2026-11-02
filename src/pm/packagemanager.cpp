#include "packagemanager.h"

#include <utils/http.h>
#include <utils/json.h>

#include <pm/steps/packagefetchstep.h>
#include <pm/steps/packageinstallstep.h>

#ifdef Q_OS_WIN
#   include <windows.h>
#endif

#include <QDir>
#include <QFile>

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

    const QString home = QDir::homePath() + "/.cspm";

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

bool PackageManager::fetchIndex()
{
    const HttpResponse response = HTTP::get(url("/index.json"));

    if (!response.success()) {
        console.err << response.errorString << Qt::endl;
        return false;
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

        const HttpResponse response = HTTP::get(url(packageRemotePath + "/meta.json"));
        if (!response.success()) {
            console.err << response.errorString << Qt::endl;
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
                response = HTTP::get(url(packageRemotePath + '/' + remoteName + ".json"));
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

QList<QPair<Package, QList<PackageVersion>>> PackageManager::search(const QString &query, const SearchOptions &options)
{
    QList<QPair<Package, QList<PackageVersion>>> results;

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

QPair<Package, QList<PackageVersion> > PackageManager::get(const QString &packageName)
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
            const PackageVersion version = JSON::make<PackageVersion>(packageDir + '/' + versionStr + ".json");
            versions.append(version);
        }
    }

    return { package, versions };
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

    PackageInstallStep install(this, command);
    return install.run(context).exitCode();
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

QUrl PackageManager::url(const QString &path) const
{
    QUrl url = registryUrl();
    url.setPath(url.path() + path);
    return url;
}