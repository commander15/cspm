#include "packagefetchstep.h"

#include <core/error.h>
#include <utils/http.h>

#include <QDir>
#include <QFileInfo>
#include <QDateTime>
#include <QCryptographicHash>

PackageFetchStep::PackageFetchStep(const QString &step, Command *command)
    : PackageStep(command)
    , m_step(step)
{}

PackageResult PackageFetchStep::run(const PackageContext &context)
{
    if (!context.cache) {
        QDir dir;

        // Cleaning up source dir
        dir.setPath(context.sourceDir);
        if (dir.exists())
            dir.removeRecursively();

        // Cleaning up build dir
        dir.setPath(context.buildDir);
        if (dir.exists())
            dir.removeRecursively();
    }

    auto download = [this, &context](const PackageArtifact &artifact, QString &fileName) {
        const QUrl url(artifact.url());
        if (!url.isValid()) {
            return PackageResult::make(CSPM_ERR_DOWNLOAD_FAILED);
        }

        fileName = context.sourceDir + '/' + url.fileName();
        return PackageFetchStep::download(url, artifact.sha256(), fileName, context.lowBandwidth);
    };

    if (m_step.startsWith("source")) {
        const PackageSource source = context.version.source();

        if (source.type == "git") {
            return clone(source.repository(), source.tag(), GitTag, context.sourceDir, context.lowBandwidth);
        }

        if (source.data.contains("url")) {
            QString fileName;

            PackageResult result = download(source, fileName);
            if (!result.success()) return result;

            return decompress(fileName, context.sourceDir);
        }
    }

    if (m_step.startsWith("binar")) {
        PackageBinary binary = context.version.binary();
        QString fileName; // Will be populated by download() on success

        PackageResult result = download(binary, fileName);
        if (!result.success()) {
            return result;
        }

        if (fileName.endsWith(".zip") || fileName.endsWith(".tar.gz")) {
            return decompress(fileName, context.installDir);
        } else {
            // Ensure the build directory exists
            QDir installDir(context.installDir);
            if (!installDir.exists() && !installDir.mkpath(".")) {
                return PackageResult::make(CSPM_ERR_PERMISSION_DENIED, "Failed to create install directory: " + context.installDir);
            }

            // For a raw binary, construct the full destination file path inside buildDir
            QFileInfo sourceInfo(fileName);
            QString targetPath = installDir.filePath(sourceInfo.fileName());

            // Remove old target if it exists so copy doesn't fail
            if (QFile::exists(targetPath)) {
                QFile::remove(targetPath);
            }

            bool copied = QFile::copy(fileName, targetPath);
            if (!copied) {
                return PackageResult::make(CSPM_ERR_PERMISSION_DENIED, "Failed to copy binary to build directory: " + targetPath);
            }
            return PackageResult::make(CSPM_OK);
        }
    }

    return PackageResult::make(CSPM_ERR_DOWNLOAD_FAILED);
}

PackageResult PackageFetchStep::clone(const QString &repo, const QString &id, GitIdType type, const QString &dir, bool lowBandwidth)
{
    // 1. Check if the repository is already cached/cloned locally
    if (QDir(dir + "/.git").exists()) {
        // Smart update path: fetch updates without re-cloning
        PackageResult fetch = gitFetch(repo, id, type, dir, lowBandwidth);
        if (fetch.exitCode() != 0) {
            return PackageResult::make(CSPM_ERR_GIT_CLONE_FAILED, "Git fetch failed for cached repository.");
        }

        // Checkout the requested tag, branch, or commit
        QProcess *checkout = runProgram("git", QStringList() << "-C" << dir << "checkout" << "-f" << id);
        if (checkout->exitCode() != 0) {
            return PackageResult::make(CSPM_ERR_GIT_CHECKOUT_FAILED, "Failed to checkout ref: " + id);
        }

        return PackageResult::make(CSPM_OK);
    }

    // 2. Fresh clone path (when directory doesn't exist yet)
    PackageResult result = (lowBandwidth ? cloneSlowly(repo, id, type, dir) : cloneFastly(repo, id, type, dir));
    if (!result.success()) {
        QDir d(dir);
        if (d.exists())
            d.removeRecursively();
    }
    return result;
}

PackageResult PackageFetchStep::download(const QUrl &url, const QString &outputDir, bool lowBandwidth)
{
    return download(url, QString(), outputDir + '/' + url.fileName(), lowBandwidth);
}

PackageResult PackageFetchStep::download(const QUrl &url, const QString &sha, const QString &fileName, bool lowBandwidth)
{
    // 1. Ensure the containing directory exists before attempting to open the file
    QFileInfo fileInfo(fileName);
    QDir dir = fileInfo.dir();
    if (!dir.exists() && !dir.mkpath(".")) {
        return PackageResult::make(CSPM_ERR_PERMISSION_DENIED, "Failed to create containing directory: " + dir.absolutePath());
    }

    // 2. Low-bandwidth caching check: if enabled, check if the file exists and is recent
    // (Considered valid if it exists and is less than 24 hours old)
    bool useCached = false;
    if (lowBandwidth && fileInfo.exists()) {
        QDateTime lastModified = fileInfo.lastModified();
        if (lastModified.secsTo(QDateTime::currentDateTime()) < 86400) { // 24 hours threshold
            useCached = true;
        }
    }

    // 3. If using cache, verify its SHA checksum if provided, then return success early
    if (useCached) {
        if (!sha.isEmpty()) {
            QFile cachedFile(fileName);
            if (cachedFile.open(QIODevice::ReadOnly)) {
                QByteArray calculatedHash = QCryptographicHash::hash(cachedFile.readAll(), QCryptographicHash::Sha256);
                cachedFile.close();

                QString actualHashHex = QString::fromLatin1(calculatedHash.toHex());
                if (actualHashHex.compare(sha, Qt::CaseInsensitive) == 0) {
                    return PackageResult::make(CSPM_OK); // Valid cache hit!
                }
            }
        } else {
            return PackageResult::make(CSPM_OK); // Valid cache hit without checksum constraint
        }
    }

    // 4. Normal mode or cache miss/expired: download fresh copy (always overrides)
    QFile file(fileName);
    if (!file.open(QIODevice::ReadWrite | QIODevice::WriteOnly)) {
        return PackageResult::make(CSPM_ERR_FILE_IO, "Failed to open target file for writing: " + file.errorString());
    }

    PackageResult result = download(url, sha, &file, lowBandwidth);

    if (result.success()) {
        file.flush();
        file.close();
    } else {
        file.remove();
    }

    return result;
}

PackageResult PackageFetchStep::download(const QUrl &url, const QString &sha, QIODevice *device, bool lowBandWidth)
{
    const HttpResponse response = HTTP::get(url, device, console);

    if (!response.success()) {
        return PackageResult::make(CSPM_ERR_HTTP_STATUS, "Download failed from URL: " + url.toString() + " (" + response.errorString + ")");
    }

    // Verify SHA checksum if provided directly from the device
    if (!sha.isEmpty()) {
        // Ensure we can read from the device from the beginning
        if (!device->isReadable()) {
            return PackageResult::make(CSPM_ERR_FILE_IO, "Device is not open for reading/verification.");
        }

        // Seek back to the start of the written data
        if (device->isSequential()) {
            return PackageResult::make(CSPM_ERR_FILE_IO, "Device is sequential and cannot be rewound for checksum verification.");
        }
        device->seek(0);

        QByteArray calculatedHash = QCryptographicHash::hash(device->readAll(), QCryptographicHash::Sha256);

        // Rewind again so that subsequent steps (like extraction) can read from the beginning of the device
        device->seek(0);

        QString actualHashHex = QString::fromLatin1(calculatedHash.toHex());
        if (actualHashHex.compare(sha.contains(':') ? sha.section(':', -1) : sha, Qt::CaseInsensitive) != 0) {
            return PackageResult::make(CSPM_ERR_CHECKSUM_MISMATCH, QString("Checksum verification failed!\nExpected: %1\nGot: %2")
                                           .arg(sha, actualHashHex));
        }
    }

    return PackageResult::make(CSPM_OK);
}

PackageResult PackageFetchStep::decompress(const QString &fileName, const QString &outputDir)
{
    QDir dir(outputDir);
    if (!dir.exists() && !dir.mkpath(".")) {
        return PackageResult::make(CSPM_ERR_PERMISSION_DENIED, "Failed to create output directory: " + outputDir);
    }

    // 1. Extract the outer archive
    QStringList args;
    args << "x" << fileName << "-y" << QString("-o%1").arg(outputDir);

    QProcess *tool = runProgram("7z", args);
    if (!tool || tool->exitCode() != 0) {
        return PackageResult::make(CSPM_ERR_EXTRACT_FAILED, "Failed to extract archive using 7z.");
    }

    // 2. Handle inner .tar if it was a .tar.gz / .tgz
    if (fileName.endsWith(".tar.gz", Qt::CaseInsensitive) || fileName.endsWith(".tgz", Qt::CaseInsensitive)) {
        QFileInfo fileInfo(fileName);
        QString tarFileName = fileInfo.completeBaseName(); // e.g., foo.tar
        QString innerTarPath = dir.filePath(tarFileName);

        if (QFile::exists(innerTarPath)) {
            QStringList tarArgs;
            tarArgs << "x" << innerTarPath << "-y" << QString("-o%1").arg(outputDir);

            QProcess *tarTool = runProgram("7z", tarArgs);
            if (tarTool && tarTool->exitCode() == 0) {
                QFile::remove(innerTarPath); // Clean up intermediate .tar
            } else {
                return PackageResult::make(CSPM_ERR_EXTRACT_FAILED, "Failed to extract inner tar archive.");
            }
        }
    }

    // 3. Flattening check: If extraction created a single top-level directory, lift its contents up
    dir.refresh();
    QStringList entries = dir.entryList(QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot);

    // If there is only ONE entry in outputDir, and that entry is a directory...
    if (entries.size() == 1) {
        QString subDirPath = dir.filePath(entries.first());
        QFileInfo subDirInfo(subDirPath);

        if (subDirInfo.isDir()) {
            QDir subDir(subDirPath);
            QStringList subEntries = subDir.entryList(QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot);

            // Move everything from the subdirectory up to outputDir
            for (const QString &entry : std::as_const(subEntries)) {
                QString oldPath = subDir.filePath(entry);
                QString newPath = dir.filePath(entry);

                // Remove existing destination if it clashes
                if (QFileInfo(newPath).isDir()) {
                    QDir(newPath).removeRecursively();
                } else {
                    QFile::remove(newPath);
                }

                QFile::rename(oldPath, newPath);
            }

            // Remove the now-empty wrapper subdirectory
            subDir.removeRecursively();
        }
    }

    return PackageResult::make(CSPM_OK);
}

PackageFetchStep *PackageFetchStep::make(const QString &step, Command *command)
{
    return new PackageFetchStep(step.isEmpty() ? "sources" : step.toLower(), command);
}

PackageResult PackageFetchStep::gitFetch(const QString &repo, const QString &id, GitIdType type, const QString &dir, bool lowBandwidth)
{
    int maxRetries = 3;
    bool fetchSuccess = false;

    QStringList fetchArgs;
    fetchArgs << "-C" << dir;
    if (lowBandwidth)
        fetchArgs << "-c" << "http.lowSpeedLimit=500" << "-c" << "http.lowSpeedTime=60";
    fetchArgs << "fetch" << "--depth" << "1" << "origin";

    // If it's a specific branch/tag, fetch only that ref. If it's a commit hash, fetch it directly.
    if (!id.isEmpty()) {
        fetchArgs << id;
    }

    for (int attempt = 1; attempt <= maxRetries; ++attempt) {
        QProcess *fallbackFetch = runProgram("git", fetchArgs);
        if (fallbackFetch->exitCode() == 0) {
            fetchSuccess = true;
            break;
        }
    }

    if (!fetchSuccess) {
        return PackageResult::make(CSPM_ERR_GIT_CHECKOUT_FAILED, "Resilient Fallback: Network failed repeatedly during chunked fetch.");
    }

    return PackageResult::make(CSPM_OK);
}

PackageResult PackageFetchStep::cloneFastly(const QString &repo, const QString &id, GitIdType type, const QString &dir)
{
    QStringList args;
    args << "clone" << "--single-branch";

    // If it's a tag or branch, optimize by cloning just that ref with depth 1
    if (type == GitTag || type == GitBranch) {
        args << "--branch" << id << "--depth" << "1";
    }

    args << repo << dir;

    QProcess *git = runProgram("git", args);
    if (git->exitCode() != 0) {
        return PackageResult::make(CSPM_ERR_GIT_CLONE_FAILED, "Git clone failed with exit code " + QString::number(git->exitCode()));
    }

    // If it's a specific commit hash, git clone --branch won't work directly,
    // so we clone normally and then check out the commit hash.
    if (type == GitCommit && !id.isEmpty()) {
        QProcess *checkout = runProgram("git", QStringList() << "-C" << dir << "checkout" << id);
        if (checkout->exitCode() != 0) {
            return PackageResult::make(CSPM_ERR_GIT_CHECKOUT_FAILED, "Failed to checkout commit hash: " + id);
        }
    }

    return PackageResult::make(CSPM_OK);
}

PackageResult PackageFetchStep::cloneSlowly(const QString &repo, const QString &id, GitIdType type, const QString &dir)
{
    QDir().mkdir(dir);

    // Step A: Initialize repository manually
    if (runProgram("git", QStringList() << "-C" << dir << "init" << "-b" << "main")->exitCode() != 0) {
        return PackageResult::make(CSPM_ERR_GIT_CLONE_FAILED, "Resilient Fallback: git init failed.");
    }

    // Step B: Set up remote origin with network tuning flags for poor connections
    QStringList remoteArgs;
    remoteArgs << "-C" << dir << "-c" << "http.lowSpeedLimit=500" << "-c" << "http.lowSpeedTime=60"
               << "remote" << "add" << "origin" << repo;
    if (runProgram("git", remoteArgs)->exitCode() != 0) {
        return PackageResult::make(CSPM_ERR_GIT_CLONE_FAILED, "Resilient Fallback: git remote add failed.");
    }

    // Step C: Execute step-by-step fetching with retry loop
    PackageResult result = gitFetch(repo, id, type, dir, true);
    if (!result.success())
        return result;

    // For tags, branches, or commits, if we fetched a specific ID,
    // it lives in FETCH_HEAD.
    QString checkoutRef = !id.isEmpty() ? "FETCH_HEAD" : id;

    QProcess *checkout = runProgram("git", QStringList() << "-C" << dir << "checkout" << "-f" << checkoutRef);
    if (checkout->exitCode() != 0) {
        return PackageResult::make(CSPM_ERR_GIT_CHECKOUT_FAILED, "Failed to checkout final target ref: " + id);
    }

    return PackageResult::make(CSPM_OK);
}