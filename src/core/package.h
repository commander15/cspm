#ifndef PACKAGE_H
#define PACKAGE_H

#include <QObject>
#include <QVariantHash>

class Package
{
    Q_GADGET
    Q_PROPERTY(QString name MEMBER name FINAL)
    Q_PROPERTY(QString displayName MEMBER displayName FINAL)
    Q_PROPERTY(QString description MEMBER description FINAL)

public:
    QString name;
    QString displayName;
    QString description;
};

class PackageDependency
{
    Q_GADGET
    Q_PROPERTY(QString version MEMBER version FINAL)
    Q_PROPERTY(QString type MEMBER type FINAL)

public:
    QString name() const { return names.value("default"); }

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
    QString url() const;
    QString sha256() const;
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
};

class PackageVersion
{
    Q_GADGET
    Q_PROPERTY(QString version MEMBER version FINAL)

public:
    QString version;
    QList<PackageDependency> dependencies;
    PackageBuildData build;
    QList<PackageArtifact> artifacts;

    Q_INVOKABLE void fill(const QJsonObject &data);
};

#endif // PACKAGE_H
