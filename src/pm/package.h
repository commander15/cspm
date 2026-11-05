#ifndef PACKAGE_H
#define PACKAGE_H

#include <core/error.h>

#include <QObject>
#include <QVariantHash>

class Package
{
    Q_GADGET
    Q_PROPERTY(QString name MEMBER name FINAL)
    Q_PROPERTY(QString displayName MEMBER displayName FINAL)
    Q_PROPERTY(QString cmakeName MEMBER cmakeName FINAL)
    Q_PROPERTY(QString description MEMBER description FINAL)

public:
    int id = 0;
    QString name;
    QString displayName;
    QString cmakeName;
    QString description;

    Q_INVOKABLE void fill(const QJsonObject &data);
};

class PackageDependency
{
    Q_GADGET
    Q_PROPERTY(QString version MEMBER version FINAL)
    Q_PROPERTY(QString type MEMBER type FINAL)

public:
    QString name() const { return names.value("default"); }

    int id = 0;
    QHash<QString, QString> names;
    QString version;
    QString type;

    Q_INVOKABLE void fill(const QJsonObject &data);
};

class PackageArtifact
{
    Q_GADGET
    Q_PROPERTY(QString name MEMBER name FINAL)
    Q_PROPERTY(QVariantHash data MEMBER data FINAL)
    Q_PROPERTY(QString type MEMBER type FINAL)
    Q_CLASSINFO("generic", "true")

public:
    QString name;
    QVariantHash data;
    QString type;

    QString url() const;

    QString sha256() const;
    QString checksum() const;
    QString checksumAlgorithm() const;

    bool hasData(const QString &key) const
    { return data.contains(key); }

    class PackageSource toSource() const;
    class PackageBinary toBinary() const;

    template<typename T>
    T to() const {
        T target;
        static_cast<PackageArtifact &>(target) = *this;
        return target;
    }

    Q_INVOKABLE void fill(const QJsonObject &data);
};

class PackageSource : public PackageArtifact
{
    Q_GADGET

public:
    QString repository() const { return data.value("repository").toString(); }

    bool hasTag() const { return hasData("tag"); }
    QString tag() const { return data.value("tag").toString(); }
};

class PackageBinary : public PackageArtifact
{
    Q_GADGET

public:
    QString system() const;
    QString arch() const;
    QString toolchain() const;
};

class PackageBuildData
{
    Q_GADGET
    Q_PROPERTY(QString generator MEMBER generator FINAL)

public:
    QString generator;
    QHash<QString, QStringList> targets;
    QVariantHash variables;
    QStringList arguments;
    QVariantHash environment;
    QList<PackageDependency> dependencies;

    Q_INVOKABLE void fill(const QJsonObject &data);

    static PackageBuildData merge(const PackageBuildData &base, const PackageBuildData &other);
};

class PackageVersion
{
    Q_GADGET
    Q_PROPERTY(QString version MEMBER version FINAL)

public:
    int id = 0;
    QString version;
    QList<PackageDependency> dependencies;
    struct { PackageBuildData runtime; PackageBuildData dev; } build;
    QList<PackageArtifact> artifacts;

    PackageSource source() const;
    PackageBinary binary() const;
    QList<PackageBinary> binaries() const;

    Q_INVOKABLE void fill(const QJsonObject &data);
};

class PackageContext {
public:
    Package package;
    PackageVersion version;

    QHash<QString, QString> environment;

    QString sourceDir;
    QString buildDir;
    QString installDir;
    QString packageDir;

    bool dev = false;
    bool cache = true;
    bool lowBandwidth = false;
};


class PackageResult
{
public:
    bool success() const { return m_exitCode == 0; }
    int exitCode() const { return m_exitCode; }
    PackageResult &exit(int exitCode) { m_exitCode = exitCode; return *this; }

    QString message() const { return (m_messages.isEmpty() ? Error::errorString(m_exitCode) : m_messages.first()); }
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

#endif // PACKAGE_H
