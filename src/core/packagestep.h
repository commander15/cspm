#ifndef PACKAGESTEP_H
#define PACKAGESTEP_H

#include <core/command.h>
#include <core/package.h>

#include <QString>
#include <QProcess>

class PackageContext;
class PackageResult;

class PackageStep
{
public:
    PackageStep(Command *command);
    virtual ~PackageStep() = default;

    virtual PackageResult run(const PackageContext &context) = 0;

protected:
    QProcess *runProgram(const QString &program, const QStringList &arguments, const QString &workingDir = QString());
    QProcess *runProgram(const QString &program, const QStringList &arguments, const QProcessEnvironment &environment, const QString &workingDir = QString());

    ConsoleStreams console;
};

class PackageContext {
public:
    Package package;
    PackageVersion version;

    QString sourceDir;
    QString buildDir;
    QString installDir;
};

class PackageResult
{
public:
    bool success() const { return m_exitCode == 0; }
    int exitCode() const { return m_exitCode; }
    PackageResult &exit(int exitCode) { m_exitCode = exitCode; return *this; }

    QString message() const { return (m_messages.isEmpty() ? QString() : m_messages.first()); }
    PackageResult &message(const QString &msg) { m_messages = { msg }; return *this; }

    QStringList messages() const { return m_messages; }
    PackageResult &messages(const QStringList &msgs) { m_messages = msgs; return *this; }

    static PackageResult make(int code) { return PackageResult().exit(code); }
    static PackageResult make(int code, const QString &msg) { return PackageResult().exit(code).message(msg); }
    static PackageResult make(int code, const QStringList &msgs) { return PackageResult().exit(code).messages(msgs); }

private:
    int m_exitCode = 0;
    QStringList m_messages;
};

#endif // PACKAGESTEP_H
