#include "db.h"

#include <core/command.h>

#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QSqlRecord>
#include <QFile>

bool DB::beginTransaction()
{
    return QSqlDatabase::database().transaction();
}

bool DB::commitTransaction()
{
    return QSqlDatabase::database().commit();
}

bool DB::rollbackTransaction()
{
    return QSqlDatabase::database().rollback();
}

QSqlError DB::select(const QString &statement, const QVariantList &params, const std::function<void (const QVariantHash &)> &callback)
{
    const QString finalStatement = (statement.startsWith("SELECT", Qt::CaseInsensitive) ? statement : "SELECT " + statement);
    QSqlQuery query = run(finalStatement, params, ConsoleStreams::null());
    while (query.next()) {
        QVariantHash hash;

        const QSqlRecord rec = query.record();
        for (int i(0); rec.count(); ++i)
            hash.insert(rec.fieldName(i), rec.value(i));

        callback(hash);
    }
    return query.lastError();
}

QSqlError DB::insert(const QString &table, const QVariantMap &data, int *id)
{
    const QStringList columns = data.keys();
    const QStringList values(columns.size(), "?");

    QString statement = "INSERT INTO " + table + "(" + columns.join(", ") + ") VALUES(" + values.join(", ") + ")";
    QSqlQuery query = run(statement, data.values(), ConsoleStreams::null());
    return query.lastError();
}

QSqlError DB::update(const QString &table, const QVariantMap &data, const QString &filter, int *count)
{
    return QSqlError();
}

QSqlError DB::deleteData(const QString &table, const QString &filter, int *count)
{
    QString statement = "DELETE FROM " + table;
    if (!filter.isEmpty())
        statement.append(" WHERE " + filter);
    return run(statement, ConsoleStreams::null()).lastError();
}

QSqlQuery DB::run(const QString &statement)
{
    QSqlQuery query(statement);
    return run(query, ConsoleStreams::null());
}

QSqlQuery DB::run(const QString &statement, const ConsoleStreams &console)
{
    QSqlQuery query(statement);
    return run(query, console);
}

QSqlQuery DB::run(const QString &statement, const QVariantList &params, const ConsoleStreams &console)
{
    QSqlQuery query;
    query.prepare(statement);

    for (int i(0); i < params.size(); ++i)
        query.bindValue(i, params.at(i));

    query.exec();
    return run(query, console);
}

QSqlQuery DB::run(QSqlQuery &query, const ConsoleStreams &console)
{
    const QSqlError error = query.lastError();
    if (error.isValid())
        console.err << error.text() << Qt::endl;
    return std::move(query);
}

bool DB::runMigrations()
{
    QSqlDatabase db = QSqlDatabase::database();
    QSqlQuery sql(db);

    bool hasNext = true;
    if (!db.tables().contains("migrations") && !runMigration(0, &hasNext)) {
        return false;
    }

    sql = run("SELECT MAX(idx) FROM migrations");
    if (!sql.next()) {
        return false;
    }

    int lastRanMingration = sql.value(0).toInt();
    for (int i(lastRanMingration + 1); hasNext; ++i) {
        if (!runMigration(i, &hasNext))
            return false;
    }

    return true;
}

bool DB::runMigration(int index, bool *hasNext)
{
    const ConsoleStreams console = ConsoleStreams::std();
    QSqlQuery query;

    const QStringList statements = readMigration(index, hasNext);
    for (const QString &statement : statements) {
        query = run(statement);
        if (query.lastError().isValid()) {
            console.err << query.lastError().text() << Qt::endl;
            return false;
        }
    }

    query = run("INSERT INTO migrations(idx) VALUES (?)", { index }, console);
    return !query.lastError().isValid();
}

QStringList DB::readMigration(int index, bool *hasNext)
{
    static const QStringList migrationFiles = {
        ":/db/migrations/00_initial_migration.sql"
    };

    if (index >= migrationFiles.size()) {
        if (hasNext) *hasNext = false;
        return QStringList();
    }

    QFile f(migrationFiles.at(index));
    if (!f.open(QIODevice::ReadOnly)) {
        if (hasNext) *hasNext = false;
        return QStringList();
    }

    if (hasNext)
        *hasNext = (index < migrationFiles.size() - 1);

    return readSql(f.readAll());
}

QStringList DB::readSql(const QByteArray &data)
{
    QStringList queries;
    QByteArrayList lines = data.split('\n');

    QString currentQuery;
    for (const QByteArray &lineBytes : std::as_const(lines)) {
        QString line = QString::fromUtf8(lineBytes).trimmed();

        // Skip empty lines and full-line comments
        if (line.isEmpty() || line.startsWith("--")) {
            continue;
        }

        // Append line content with a space to prevent word merging across line breaks
        currentQuery.append(line);
        currentQuery.append("\n");

        // If the line terminates with a semicolon, finalize the statement
        if (line.endsWith(';')) {
            currentQuery.removeLast();
            queries.append(currentQuery);
            currentQuery.clear();
        }
    }

    // Catch any trailing query that might lack a final semicolon
    QString leftover = currentQuery.trimmed();
    if (!leftover.isEmpty()) {
        queries.append(leftover);
    }

    return queries;
}
