#ifndef DB_H
#define DB_H

#include <QString>
#include <QSqlQuery>
#include <QSqlError>

class ConsoleStreams;

class DB
{
public:
    class Transaction {
    public:
        Transaction() : m_commit(false) { DB::beginTransaction(); }
        ~Transaction() { if (m_commit) DB::commitTransaction(); else DB::rollbackTransaction(); }
        void commit(bool confirm = true) { m_commit = confirm; }
    private:
        bool m_commit;
    };

    static bool beginTransaction();
    static bool commitTransaction();
    static bool rollbackTransaction();

    static QSqlError select(const QString &statement, const QVariantList &params, const std::function<void(const QVariantHash &record)> &callback);
    static QSqlError insert(const QString &table, const QVariantMap &data, int *id = nullptr);
    static QSqlError update(const QString &table, const QVariantMap &data, const QString &filter, int *count = nullptr);
    static QSqlError deleteData(const QString &table, const QString &filter, int *count = nullptr);

    static QSqlQuery run(const QString &statement);
    static QSqlQuery run(const QString &statement, const ConsoleStreams &console);
    static QSqlQuery run(const QString &statement, const QVariantList &params, const ConsoleStreams &console);
    static QSqlQuery run(QSqlQuery &query, const ConsoleStreams &console);

    static bool runMigrations();
    static bool runMigration(int index, bool *hasNext = nullptr);
    static QStringList readMigration(int index, bool *hasNext = nullptr);
    static QStringList readSql(const QByteArray &data);
};

#endif // DB_H
