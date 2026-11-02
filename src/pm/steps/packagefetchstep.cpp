#include "packagefetchstep.h"

#include <QDir>

PackageFetchStep::PackageFetchStep(Command *command)
    : PackageStep(command)
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

    const PackageSource source = context.version.source();

    if (source.type == "git") {
        return clone(source.repository(), source.tag(), GitTag, context.sourceDir, context.lowBandwidth);
    }

    return PackageResult::make(-1);
}

PackageResult PackageFetchStep::clone(const QString &repo, const QString &id, GitIdType type, const QString &dir, bool lowBandwidth)
{
    // 1. Check if the repository is already cached/cloned locally
    if (QDir(dir + "/.git").exists()) {
        // Smart update path: fetch updates without re-cloning
        PackageResult fetch = gitFetch(repo, id, type, dir, lowBandwidth);
        if (fetch.exitCode() != 0) {
            return PackageResult::make(fetch.exitCode(), "Git fetch failed for cached repository.");
        }

        // Checkout the requested tag, branch, or commit
        QProcess *checkout = runProgram("git", QStringList() << "-C" << dir << "checkout" << "-f" << id);
        if (checkout->exitCode() != 0) {
            return PackageResult::make(checkout->exitCode(), "Failed to checkout ref: " + id);
        }

        return PackageResult::make(0);
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
        return PackageResult::make(-1, "Resilient Fallback: Network failed repeatedly during chunked fetch.");
    }

    return PackageResult::make(0);
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
        return PackageResult::make(git->exitCode(), "Git clone failed with exit code " + QString::number(git->exitCode()));
    }

    // If it's a specific commit hash, git clone --branch won't work directly,
    // so we clone normally and then check out the commit hash.
    if (type == GitCommit && !id.isEmpty()) {
        QProcess *checkout = runProgram("git", QStringList() << "-C" << dir << "checkout" << id);
        if (checkout->exitCode() != 0) {
            return PackageResult::make(checkout->exitCode(), "Failed to checkout commit hash: " + id);
        }
    }

    return PackageResult::make(0);
}

PackageResult PackageFetchStep::cloneSlowly(const QString &repo, const QString &id, GitIdType type, const QString &dir)
{
    QDir().mkdir(dir);

    // Step A: Initialize repository manually
    if (runProgram("git", QStringList() << "-C" << dir << "init" << "-b" << "main")->exitCode() != 0) {
        return PackageResult::make(-1, "Resilient Fallback: git init failed.");
    }

    // Step B: Set up remote origin with network tuning flags for poor connections
    QStringList remoteArgs;
    remoteArgs << "-C" << dir << "-c" << "http.lowSpeedLimit=500" << "-c" << "http.lowSpeedTime=60"
               << "remote" << "add" << "origin" << repo;
    if (runProgram("git", remoteArgs)->exitCode() != 0) {
        return PackageResult::make(-1, "Resilient Fallback: git remote add failed.");
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
        return PackageResult::make(checkout->exitCode(), "Failed to checkout final target ref: " + id);
    }

    return PackageResult::make(0);
}