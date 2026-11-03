#include "packageinstallstep.h"

#include <core/error.h>
#include <pm/packagemanager.h>
#include <pm/steps/packagefetchstep.h>

#include <QEventLoop>
#include <QDir>

PackageInstallStep::PackageInstallStep(PackageManager *manager, Command *command)
    : PackageStep(command)
    , manager(manager)
    , builder(command)
    , fetch("binary", command)
{}

PackageResult PackageInstallStep::run(const PackageContext &context)
{
    console.out << Qt::endl; // Add a line between title and build output

    if (context.dev)
        return buildAndInstall(context);

    const PackageBinary binary = context.version.binary();
    if (!binary.name.isEmpty())
        return decompressAndInstall(context, binary);

    return buildAndInstall(context);
}

PackageResult PackageInstallStep::buildAndInstall(PackageContext context)
{
    QHash<QString, QString> env;

    // CMake prefix path update
    const QStringList cmakePrefixPaths = { };
    if (!cmakePrefixPaths.isEmpty()) {
        if (qEnvironmentVariableIsEmpty("CMAKE_PREFIX_PATH")) {
            env.insert("CMAKE_PREFIX_PATH", cmakePrefixPaths.join(";"));
        } else {
            env.insert("CMAKE_PREFIX_PATH", qEnvironmentVariable("CMAKE_PREFIX_PATH") + ';' + cmakePrefixPaths.join(";"));
        }
    }

    context.environment.insert(env);

    PackageBuild *build = builder.build(context);
    QEventLoop loop;

    QObject::connect(build, &PackageBuild::finished, &loop, [build, &loop](const PackageResult &result) {
        build->deleteLater();
        loop.exit(result.exitCode());
    });

    int result = loop.exec();
    console.out << Qt::endl; // Add a line between build output and step footer

    // Register on CMake User Registry
    if (result == 0)
        manager->registerWithCMakeUserRegistry(context.package, context.version);

    return finalizeInstall(context.installDir);
}

PackageResult PackageInstallStep::decompressAndInstall(const PackageContext &context, const PackageBinary &binary)
{
    PackageResult result = PackageStep::runOne(context, &fetch, console);
    return (result.success() ? finalizeInstall(context.installDir) : result);
}

PackageResult PackageInstallStep::finalizeInstall(const QString &prefixPath)
{
    // admin flag will help switch between root/admin install and user install (reserved for future usage)
    const bool admin = false;

    QDir prefixDir(prefixPath);
    if (!prefixDir.exists()) {
        return PackageResult::make(1, "Prefix path does not exist: " + prefixPath);
    }

    // Determine user-level target installation directory (no root/admin required)
    QString userBinDir;
#if defined(Q_OS_LINUX)
    userBinDir = QDir::homePath() + "/.local/bin";
#elif defined(Q_OS_WIN)
    userBinDir = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + "/cspm/bin";
#else
    userBinDir = prefixDir.filePath("bin");
#endif

    QDir targetDir(userBinDir);
    if (!targetDir.exists() && !targetDir.mkpath(".")) {
        return PackageResult::make(1, "Failed to create user binary installation directory: " + userBinDir);
    }

    // Locate binaries: check if a 'bin' subdirectory exists, otherwise scan root of prefix
    QStringList searchDirs;
    if (QDir(prefixDir.filePath("bin")).exists()) {
        searchDirs << prefixDir.filePath("bin");
    }
    searchDirs << prefixPath; // Also check root in case binaries were flattened there

    int installedCount = 0;

    for (const QString &searchPath : std::as_const(searchDirs)) {
        QDir binDir(searchPath);
        QFileInfoList fileList = binDir.entryInfoList(QDir::Files | QDir::NoDotAndDotDot);

        for (const QFileInfo &fileInfo : std::as_const(fileList)) {
            QString filePath = fileInfo.absoluteFilePath();
            QString fileName = fileInfo.fileName();
            QString targetPath = targetDir.filePath(fileName);

#if defined(Q_OS_LINUX)
            // On Linux, symlink binaries for zero-duplication and clean updates
            if (QFile::exists(targetPath)) {
                QFile::remove(targetPath);
            }
            if (QFile::link(filePath, targetPath)) {
                // Ensure executable permissions on the target/symlink
                QFile::setPermissions(targetPath, QFile::Permissions(QFile::ExeOwner | QFile::ReadOwner |
                                                                     QFile::ExeUser | QFile::ReadUser |
                                                                     QFile::ExeGroup | QFile::ReadGroup));
                installedCount++;
            }
#elif defined(Q_OS_WIN)
            // On Windows, copy executable and library files (.exe, .dll, or extensionless binaries)
            QString suffix = fileInfo.suffix().toLower();
            if (suffix == "exe" || suffix == "dll" || suffix.isEmpty()) {
                if (QFile::exists(targetPath)) {
                    QFile::remove(targetPath);
                }
                if (QFile::copy(filePath, targetPath)) {
                    installedCount++;
                }
            }
#endif
        }
    }

    if (installedCount == 0) {
        return PackageResult::make(0, "No executable binaries found to install in prefix: " + prefixPath);
    }

    // Register user PATH on Windows without requiring Administrator privileges
#if defined(Q_OS_WIN)
    QString normalizedUserBinDir = QDir::toNativeSeparators(userBinDir);
    QString powershellCmd = QString(
                                "$userPath = [Environment]::GetEnvironmentVariable('PATH', 'User'); "
                                "if ($userPath -notlike '*%1*') { "
                                "  [Environment]::SetEnvironmentVariable('PATH', $userPath + ';%1', 'User'); "
                                "}"
                                ).arg(normalizedUserBinDir);

    QProcess::execute("powershell", QStringList() << "-NoProfile" << "-Command" << powershellCmd);
#endif

    return PackageResult::make(0);
}

PackageResult PackageInstallStep::finalizeUninstall(const QString &prefixPath)
{
    QDir prefixDir(prefixPath);
    if (!prefixDir.exists()) {
        // If the prefix path is already gone, nothing left to map and remove
        return PackageResult::make(0);
    }

    // Determine the same user-level target installation directory used during install
    QString userBinDir;
#if defined(Q_OS_LINUX)
    userBinDir = QDir::homePath() + "/.local/bin";
#elif defined(Q_OS_WIN)
    userBinDir = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + "/cspm/bin";
#else
    userBinDir = prefixDir.filePath("bin");
#endif

    QDir targetDir(userBinDir);
    if (!targetDir.exists()) {
        return PackageResult::make(0); // Nothing to clean up
    }

    // Locate binaries in the prefix to know exactly what to remove
    QStringList searchDirs;
    if (QDir(prefixDir.filePath("bin")).exists()) {
        searchDirs << prefixDir.filePath("bin");
    }
    searchDirs << prefixPath; // Check root as well

    int removedCount = 0;

    for (const QString &searchPath : std::as_const(searchDirs)) {
        QDir binDir(searchPath);
        QFileInfoList fileList = binDir.entryInfoList(QDir::Files | QDir::NoDotAndDotDot);

        for (const QFileInfo &fileInfo : std::as_const(fileList)) {
            QString fileName = fileInfo.fileName();
            QString targetPath = targetDir.filePath(fileName);

            // Remove the symlink (Linux) or copied binary/DLL (Windows) if it exists
            if (QFile::exists(targetPath) || QFileInfo(targetPath).isSymLink()) {
                if (QFile::remove(targetPath)) {
                    removedCount++;
                }
            }
        }
    }

    // Prevent orphan directories: if our user bin directory is now completely empty, clean it up!
    if (targetDir.entryList(QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot).isEmpty()) {
        targetDir.removeRecursively();

        // On Windows, if cspm/bin parent directory (cspm) is also empty, clean that up too
#if defined(Q_OS_WIN)
        QDir parentDir(targetDir);
        if (parentDir.cdUp() && parentDir.dirName() == "cspm") {
            if (parentDir.entryList(QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot).isEmpty()) {
                parentDir.removeRecursively();
            }
        }
#endif
    }

    return PackageResult::make(0);
}

PackageInstallStep *PackageInstallStep::make(PackageManager *manager, Command *command)
{
    return new PackageInstallStep(manager, command);
}