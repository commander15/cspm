#include "packagefetchstep.h"

#include <QDir>

PackageFetchStep::PackageFetchStep(Command *command)
    : PackageStep(command)
{}

PackageResult PackageFetchStep::run(const PackageContext &context)
{
    auto it = std::find_if(context.version.artifacts.begin(), context.version.artifacts.end(), [](const PackageArtifact &artifact) {
        return artifact.name == "source";
    });

    if (it == context.version.artifacts.end())
        return PackageResult::make(1);

    // We remove the source dir (if exists)
    QDir dir(context.sourceDir);
    if (dir.exists())
        dir.removeRecursively();
    else
        dir.mkpath(".");

    PackageSource source = it->toSource();
    if (source.type == "git") {
        if (source.hasTag())
            return clone(source.repository(), source.tag(), GitTag, context.sourceDir);
        else
            return clone(source.repository(), "main", GitBranch, context.sourceDir);
    }

    return PackageResult::make(2);
}

PackageResult PackageFetchStep::clone(const QString &repo, const QString &id, GitIdType type, const QString &dir)
{
    // 1. Check if the repository is already cached/cloned locally
    if (QDir(dir + "/.git").exists()) {
        // Smart update path: fetch updates without re-cloning
        QProcess *fetch = runProgram("git", QStringList() << "-C" << dir << "fetch" << "--all" << "--tags");
        if (fetch->exitCode() != 0) {
            return PackageResult::make(fetch->exitCode(), "Git fetch failed for cached repository.");
        }

        // Checkout the requested tag, branch, or commit
        QProcess *checkout = runProgram("git", QStringList() << "-C" << dir << "checkout" << "-f" << id);
        if (checkout->exitCode() != 0) {
            return PackageResult::make(checkout->exitCode(), "Failed to checkout ref: " + id);
        }

        return PackageResult::make(0);
    }

    // 2. Fresh clone path (when directory doesn't exist yet)
    QStringList args;
    args << "clone";

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
