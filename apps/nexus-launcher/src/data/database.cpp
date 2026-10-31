#include "nexus/data/database.h"

#include <QDir>
#include <QFileInfo>
#include <QSqlError>
#include <QSqlQuery>
#include <QStandardPaths>
#include <QUuid>

namespace nexus {
namespace {

QString defaultDatabasePath()
{
    const QString directory =
        QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    return QDir(directory).filePath(QStringLiteral("nexus.sqlite3"));
}

bool execute(QSqlDatabase& database, const QString& statement, QString* error)
{
    QSqlQuery query(database);
    if (query.exec(statement)) {
        return true;
    }
    if (error != nullptr) {
        *error = query.lastError().text();
    }
    return false;
}

} // namespace

Database::Database(QString databasePath)
    : databasePath_(databasePath.isEmpty() ? defaultDatabasePath() : std::move(databasePath)),
      connectionName_(QStringLiteral("nexus-%1").arg(
          QUuid::createUuid().toString(QUuid::WithoutBraces)))
{
}

Database::~Database()
{
    if (connection_.isValid()) {
        connection_.close();
        connection_ = QSqlDatabase();
        QSqlDatabase::removeDatabase(connectionName_);
    }
}

bool Database::open(QString* error)
{
    if (error != nullptr) {
        error->clear();
    }

    const QFileInfo info(databasePath_);
    if (!QDir().mkpath(info.absolutePath())) {
        if (error != nullptr) {
            *error = QStringLiteral("Could not create the database directory.");
        }
        return false;
    }

    if (!QSqlDatabase::isDriverAvailable(QStringLiteral("QSQLITE"))) {
        if (error != nullptr) {
            *error = QStringLiteral(
                "Qt's SQLite driver is unavailable. Install the Qt SQL SQLite driver plugin.");
        }
        return false;
    }

    connection_ = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName_);
    connection_.setDatabaseName(databasePath_);
    if (!connection_.open()) {
        if (error != nullptr) {
            *error = QStringLiteral("Could not open the game library database: %1")
                         .arg(connection_.lastError().text());
        }
        return false;
    }

    if (!execute(connection_, QStringLiteral("PRAGMA foreign_keys = ON"), error) ||
        !execute(connection_, QStringLiteral("PRAGMA journal_mode = WAL"), error) ||
        !migrate(error)) {
        connection_.close();
        return false;
    }
    return true;
}

QSqlDatabase Database::connection() const
{
    return connection_;
}

QString Database::filePath() const
{
    return databasePath_;
}

bool Database::migrate(QString* error)
{
    QSqlQuery versionQuery(connection_);
    if (!versionQuery.exec(QStringLiteral("PRAGMA user_version")) ||
        !versionQuery.next()) {
        if (error != nullptr) {
            *error = QStringLiteral("Could not read the database schema version: %1")
                         .arg(versionQuery.lastError().text());
        }
        return false;
    }

    const int version = versionQuery.value(0).toInt();
    if (version > 2) {
        if (error != nullptr) {
            *error = QStringLiteral(
                "This database was created by a newer Nexus version. Update the launcher "
                "before opening it.");
        }
        return false;
    }
    if (version == 2) {
        return true;
    }

    if (!connection_.transaction()) {
        if (error != nullptr) {
            *error = QStringLiteral("Could not start the database migration: %1")
                         .arg(connection_.lastError().text());
        }
        return false;
    }

    QStringList statements;
    if (version == 0) {
        statements = {
            QStringLiteral(
                "CREATE TABLE games ("
                "id INTEGER PRIMARY KEY AUTOINCREMENT,"
                "title TEXT NOT NULL,"
                "description TEXT NOT NULL DEFAULT '',"
                "executable_path TEXT NOT NULL,"
                "executable_key TEXT NOT NULL UNIQUE,"
                "working_directory TEXT NOT NULL DEFAULT '',"
                "launch_arguments TEXT NOT NULL DEFAULT '',"
                "cover_path TEXT NOT NULL DEFAULT '',"
                "launcher TEXT NOT NULL DEFAULT 'Manual',"
                "genre TEXT NOT NULL DEFAULT '',"
                "category TEXT NOT NULL DEFAULT '',"
                "collection TEXT NOT NULL DEFAULT '',"
                "franchise TEXT NOT NULL DEFAULT '',"
                "tags_json TEXT NOT NULL DEFAULT '[]',"
                "favorite INTEGER NOT NULL DEFAULT 0,"
                "launch_count INTEGER NOT NULL DEFAULT 0,"
                "total_play_seconds INTEGER NOT NULL DEFAULT 0,"
                "last_played TEXT,"
                "date_added TEXT NOT NULL,"
                "custom_order INTEGER NOT NULL DEFAULT 0"
                ")"),
            QStringLiteral("CREATE INDEX games_by_recent ON games(last_played DESC)"),
            QStringLiteral(
                "CREATE INDEX games_by_playtime ON games(total_play_seconds DESC)"),
            QStringLiteral(
                "CREATE TABLE launch_sessions ("
                "id INTEGER PRIMARY KEY AUTOINCREMENT,"
                "game_id INTEGER NOT NULL REFERENCES games(id) ON DELETE CASCADE,"
                "started_at TEXT NOT NULL,"
                "ended_at TEXT,"
                "duration_seconds INTEGER NOT NULL DEFAULT 0,"
                "exit_code INTEGER,"
                "status TEXT NOT NULL DEFAULT 'running'"
                ")"),
            QStringLiteral("CREATE INDEX sessions_by_game ON launch_sessions(game_id)"),
            QStringLiteral("PRAGMA user_version = 2"),
        };
    } else if (version == 1) {
        statements = {
            QStringLiteral("ALTER TABLE games ADD COLUMN genre TEXT NOT NULL DEFAULT ''"),
            QStringLiteral("ALTER TABLE games ADD COLUMN category TEXT NOT NULL DEFAULT ''"),
            QStringLiteral("ALTER TABLE games ADD COLUMN collection TEXT NOT NULL DEFAULT ''"),
            QStringLiteral("ALTER TABLE games ADD COLUMN franchise TEXT NOT NULL DEFAULT ''"),
            QStringLiteral("PRAGMA user_version = 2"),
        };
    }

    for (const QString& statement : statements) {
        if (!execute(connection_, statement, error)) {
            connection_.rollback();
            return false;
        }
    }

    if (!connection_.commit()) {
        if (error != nullptr) {
            *error = QStringLiteral("Could not finish the database migration: %1")
                         .arg(connection_.lastError().text());
        }
        connection_.rollback();
        return false;
    }
    return true;
}

} // namespace nexus
