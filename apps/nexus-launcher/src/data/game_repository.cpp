#include "nexus/data/game_repository.h"

#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

namespace nexus {
namespace {

QString isoDateTime(const QDateTime& value)
{
    return value.toUTC().toString(Qt::ISODateWithMs);
}

QDateTime parseDateTime(const QVariant& value)
{
    if (!value.isValid() || value.isNull()) {
        return {};
    }
    return QDateTime::fromString(value.toString(), Qt::ISODateWithMs);
}

Game gameFromQuery(const QSqlQuery& query)
{
    Game game;
    game.id = query.value(0).toLongLong();
    game.title = query.value(1).toString();
    game.description = query.value(2).toString();
    game.executablePath = query.value(3).toString();
    game.workingDirectory = query.value(4).toString();
    game.launchArguments = query.value(5).toString();
    game.coverPath = query.value(6).toString();
    game.launcher = query.value(7).toString();
    game.genre = query.value(8).toString();
    game.category = query.value(9).toString();
    game.collection = query.value(10).toString();
    game.franchise = query.value(11).toString();
    const QJsonDocument tags =
        QJsonDocument::fromJson(query.value(12).toString().toUtf8());
    for (const QJsonValue& value : tags.array()) {
        if (value.isString()) {
            game.tags.append(value.toString());
        }
    }
    game.favorite = query.value(13).toBool();
    game.launchCount = query.value(14).toLongLong();
    game.totalPlaySeconds = query.value(15).toLongLong();
    game.customOrder = query.value(16).toLongLong();
    game.lastPlayed = parseDateTime(query.value(17));
    game.dateAdded = parseDateTime(query.value(18));
    return game;
}

QString databaseError(const QSqlQuery& query)
{
    return query.lastError().text();
}

QStringList gameColumns()
{
    return {
        QStringLiteral("id"), QStringLiteral("title"), QStringLiteral("description"),
        QStringLiteral("executable_path"), QStringLiteral("working_directory"),
        QStringLiteral("launch_arguments"), QStringLiteral("cover_path"),
        QStringLiteral("launcher"), QStringLiteral("genre"), QStringLiteral("category"),
        QStringLiteral("collection"), QStringLiteral("franchise"), QStringLiteral("tags_json"),
        QStringLiteral("favorite"), QStringLiteral("launch_count"),
        QStringLiteral("total_play_seconds"), QStringLiteral("custom_order"),
        QStringLiteral("last_played"), QStringLiteral("date_added"),
    };
}

bool fail(QString* error, const QString& message)
{
    if (error != nullptr) {
        *error = message;
    }
    return false;
}

QString nonNullText(const QString& value)
{
    return value.isNull() ? QStringLiteral("") : value;
}

QJsonObject gameToJson(const Game& game)
{
    QJsonObject object;
    object.insert(QStringLiteral("title"), game.title);
    object.insert(QStringLiteral("description"), game.description);
    object.insert(QStringLiteral("executablePath"), game.executablePath);
    object.insert(QStringLiteral("workingDirectory"), game.workingDirectory);
    object.insert(QStringLiteral("launchArguments"), game.launchArguments);
    object.insert(QStringLiteral("coverPath"), game.coverPath);
    object.insert(QStringLiteral("launcher"), game.launcher);
    object.insert(QStringLiteral("genre"), game.genre);
    object.insert(QStringLiteral("category"), game.category);
    object.insert(QStringLiteral("collection"), game.collection);
    object.insert(QStringLiteral("franchise"), game.franchise);
    object.insert(QStringLiteral("favorite"), game.favorite);
    object.insert(QStringLiteral("launchCount"), game.launchCount);
    object.insert(QStringLiteral("totalPlaySeconds"), game.totalPlaySeconds);
    object.insert(QStringLiteral("lastPlayed"),
                  game.lastPlayed.isValid() ? isoDateTime(game.lastPlayed) : QString());
    object.insert(QStringLiteral("dateAdded"),
                  game.dateAdded.isValid() ? isoDateTime(game.dateAdded) : QString());
    object.insert(QStringLiteral("tags"), QJsonArray::fromStringList(game.tags));
    return object;
}

Game gameFromJson(const QJsonObject& object)
{
    Game game;
    game.title = object.value(QStringLiteral("title")).toString().trimmed();
    game.description = object.value(QStringLiteral("description")).toString();
    game.executablePath = object.value(QStringLiteral("executablePath")).toString().trimmed();
    game.workingDirectory = object.value(QStringLiteral("workingDirectory")).toString();
    game.launchArguments = object.value(QStringLiteral("launchArguments")).toString();
    game.coverPath = object.value(QStringLiteral("coverPath")).toString();
    game.launcher = object.value(QStringLiteral("launcher")).toString(QStringLiteral("Manual"));
    game.genre = object.value(QStringLiteral("genre")).toString();
    game.category = object.value(QStringLiteral("category")).toString();
    game.collection = object.value(QStringLiteral("collection")).toString();
    game.franchise = object.value(QStringLiteral("franchise")).toString();
    game.favorite = object.value(QStringLiteral("favorite")).toBool(false);
    game.launchCount = qMax<qint64>(0, object.value(QStringLiteral("launchCount")).toInteger());
    game.totalPlaySeconds =
        qMax<qint64>(0, object.value(QStringLiteral("totalPlaySeconds")).toInteger());
    game.lastPlayed = QDateTime::fromString(
        object.value(QStringLiteral("lastPlayed")).toString(), Qt::ISODateWithMs);
    game.dateAdded = QDateTime::fromString(
        object.value(QStringLiteral("dateAdded")).toString(), Qt::ISODateWithMs);
    for (const QJsonValue& value : object.value(QStringLiteral("tags")).toArray()) {
        const QString tag = value.toString().trimmed();
        if (!tag.isEmpty() && !game.tags.contains(tag, Qt::CaseInsensitive)) {
            game.tags.append(tag);
        }
    }
    return game;
}

} // namespace

GameRepository::GameRepository(Database& database)
    : database_(database)
{
}

bool GameRepository::list(QVector<Game>& games, QString* error, GameSort sort) const
{
    if (error != nullptr) {
        error->clear();
    }
    games.clear();

    QString orderBy = QStringLiteral("title COLLATE NOCASE ASC");
    switch (sort) {
    case GameSort::RecentlyPlayed:
        orderBy = QStringLiteral("last_played IS NULL ASC, last_played DESC, title COLLATE NOCASE");
        break;
    case GameSort::Playtime:
        orderBy = QStringLiteral("total_play_seconds DESC, title COLLATE NOCASE");
        break;
    case GameSort::DateAdded:
        orderBy = QStringLiteral("date_added DESC, title COLLATE NOCASE");
        break;
    case GameSort::Custom:
        orderBy = QStringLiteral("custom_order ASC, title COLLATE NOCASE");
        break;
    case GameSort::Name:
        break;
    }

    QSqlDatabase connection = database_.connection();
    QSqlQuery query(connection);
    query.prepare(QStringLiteral("SELECT %1 FROM games ORDER BY %2")
                      .arg(gameColumns().join(QLatin1Char(',')), orderBy));
    if (!query.exec()) {
        return fail(error, QStringLiteral("Could not load the library: %1")
                               .arg(databaseError(query)));
    }
    while (query.next()) {
        games.append(gameFromQuery(query));
    }
    return true;
}

bool GameRepository::add(Game game, qint64* newId, QString* error)
{
    if (error != nullptr) {
        error->clear();
    }
    game.title = game.title.trimmed();
    if (game.title.isEmpty()) {
        return fail(error, QStringLiteral("Enter a game title."));
    }

    const QFileInfo executable(game.executablePath);
    if (!executable.exists() || !executable.isFile()) {
        return fail(error, QStringLiteral("Select an existing game executable."));
    }
    game.executablePath = QDir::toNativeSeparators(
        QDir::cleanPath(executable.absoluteFilePath()));
    const QString key = normalizedExecutableKey(game.executablePath);

    QSqlDatabase connection = database_.connection();
    QSqlQuery duplicateCheck(connection);
    duplicateCheck.prepare(QStringLiteral("SELECT id FROM games WHERE executable_key = ?"));
    duplicateCheck.addBindValue(key);
    if (!duplicateCheck.exec()) {
        return fail(error, QStringLiteral("Could not check for duplicate games: %1")
                               .arg(databaseError(duplicateCheck)));
    }
    if (duplicateCheck.next()) {
        return fail(error, QStringLiteral("That executable is already in your library."));
    }

    QSqlQuery query(connection);
    query.prepare(QStringLiteral(
        "INSERT INTO games (title, description, executable_path, executable_key, "
        "working_directory, launch_arguments, cover_path, launcher, genre, category, "
        "collection, franchise, tags_json, favorite, date_added, custom_order) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, "
        "(SELECT COALESCE(MAX(custom_order) + 1, 0) FROM games))"));
    query.addBindValue(game.title);
    query.addBindValue(nonNullText(game.description));
    query.addBindValue(game.executablePath);
    query.addBindValue(key);
    query.addBindValue(nonNullText(game.workingDirectory));
    query.addBindValue(nonNullText(game.launchArguments));
    query.addBindValue(nonNullText(game.coverPath));
    query.addBindValue(game.launcher.isEmpty() ? QStringLiteral("Manual") : game.launcher);
    query.addBindValue(nonNullText(game.genre));
    query.addBindValue(nonNullText(game.category));
    query.addBindValue(nonNullText(game.collection));
    query.addBindValue(nonNullText(game.franchise));

    QJsonArray tags;
    for (const QString& tag : game.tags) {
        const QString normalizedTag = tag.trimmed();
        if (!normalizedTag.isEmpty()) {
            tags.append(normalizedTag);
        }
    }
    query.addBindValue(QString::fromUtf8(QJsonDocument(tags).toJson(QJsonDocument::Compact)));
    query.addBindValue(game.favorite);
    query.addBindValue(isoDateTime(QDateTime::currentDateTimeUtc()));

    if (!query.exec()) {
        if (query.lastError().nativeErrorCode() == QStringLiteral("19")) {
            return fail(error, QStringLiteral("That executable is already in your library."));
        }
        return fail(error, QStringLiteral("Could not add the game: %1").arg(databaseError(query)));
    }
    if (newId != nullptr) {
        *newId = query.lastInsertId().toLongLong();
    }
    return true;
}

bool GameRepository::update(const Game& game, QString* error)
{
    if (error != nullptr) {
        error->clear();
    }
    if (game.id <= 0) {
        return fail(error, QStringLiteral("The selected game is not valid."));
    }
    const QString title = game.title.trimmed();
    if (title.isEmpty()) {
        return fail(error, QStringLiteral("Enter a game title."));
    }

    const QFileInfo executable(game.executablePath);
    const QString normalizedPath = QDir::toNativeSeparators(
        QDir::cleanPath(executable.absoluteFilePath()));
    const QString key = normalizedExecutableKey(normalizedPath);

    QSqlDatabase connection = database_.connection();
    QSqlQuery duplicateCheck(connection);
    duplicateCheck.prepare(
        QStringLiteral("SELECT id FROM games WHERE executable_key = ? AND id <> ?"));
    duplicateCheck.addBindValue(key);
    duplicateCheck.addBindValue(game.id);
    if (!duplicateCheck.exec()) {
        return fail(error, QStringLiteral("Could not check for duplicate games: %1")
                               .arg(databaseError(duplicateCheck)));
    }
    if (duplicateCheck.next()) {
        return fail(error, QStringLiteral("That executable is already assigned to another game."));
    }

    QSqlQuery query(connection);
    query.prepare(QStringLiteral(
        "UPDATE games SET title = ?, description = ?, executable_path = ?, executable_key = ?, "
        "working_directory = ?, launch_arguments = ?, cover_path = ?, launcher = ?, genre = ?, "
        "category = ?, collection = ?, franchise = ?, tags_json = ? WHERE id = ?"));
    query.addBindValue(title);
    query.addBindValue(nonNullText(game.description));
    query.addBindValue(normalizedPath);
    query.addBindValue(key);
    query.addBindValue(nonNullText(game.workingDirectory));
    query.addBindValue(nonNullText(game.launchArguments));
    query.addBindValue(nonNullText(game.coverPath));
    query.addBindValue(game.launcher.isEmpty() ? QStringLiteral("Manual") : game.launcher);
    query.addBindValue(nonNullText(game.genre));
    query.addBindValue(nonNullText(game.category));
    query.addBindValue(nonNullText(game.collection));
    query.addBindValue(nonNullText(game.franchise));
    QJsonArray tags;
    for (const QString& tag : game.tags) {
        const QString normalizedTag = tag.trimmed();
        if (!normalizedTag.isEmpty()) {
            tags.append(normalizedTag);
        }
    }
    query.addBindValue(QString::fromUtf8(QJsonDocument(tags).toJson(QJsonDocument::Compact)));
    query.addBindValue(game.id);

    if (!query.exec()) {
        if (query.lastError().nativeErrorCode() == QStringLiteral("19")) {
            return fail(error, QStringLiteral("That executable is already assigned to another game."));
        }
        return fail(error, QStringLiteral("Could not update the game: %1").arg(databaseError(query)));
    }
    if (query.numRowsAffected() == 0) {
        return fail(error, QStringLiteral("This game no longer exists in the library."));
    }
    return true;
}

bool GameRepository::remove(qint64 gameId, QString* error)
{
    if (error != nullptr) {
        error->clear();
    }
    QSqlDatabase connection = database_.connection();
    QSqlQuery query(connection);
    query.prepare(QStringLiteral("DELETE FROM games WHERE id = ?"));
    query.addBindValue(gameId);
    if (!query.exec()) {
        return fail(error, QStringLiteral("Could not remove the game: %1").arg(databaseError(query)));
    }
    if (query.numRowsAffected() == 0) {
        return fail(error, QStringLiteral("This game no longer exists in the library."));
    }
    return true;
}

bool GameRepository::setFavorite(qint64 gameId, bool favorite, QString* error)
{
    if (error != nullptr) {
        error->clear();
    }
    QSqlDatabase connection = database_.connection();
    QSqlQuery query(connection);
    query.prepare(QStringLiteral("UPDATE games SET favorite = ? WHERE id = ?"));
    query.addBindValue(favorite);
    query.addBindValue(gameId);
    if (!query.exec()) {
        return fail(error, QStringLiteral("Could not update the favorite: %1").arg(databaseError(query)));
    }
    if (query.numRowsAffected() == 0) {
        return fail(error, QStringLiteral("This game no longer exists in the library."));
    }
    return true;
}

bool GameRepository::moveGame(qint64 gameId, int direction, QString* error)
{
    if (error != nullptr) {
        error->clear();
    }
    if (direction != -1 && direction != 1) {
        return fail(error, QStringLiteral("Choose an earlier or later position."));
    }

    QSqlDatabase connection = database_.connection();
    QSqlQuery query(connection);
    if (!query.exec(QStringLiteral(
            "SELECT id, custom_order FROM games ORDER BY custom_order, title COLLATE NOCASE"))) {
        return fail(error, QStringLiteral("Could not load the custom order: %1")
                               .arg(databaseError(query)));
    }
    QVector<QPair<qint64, qint64>> order;
    while (query.next()) {
        order.append(qMakePair(query.value(0).toLongLong(),
                               query.value(1).toLongLong()));
    }
    query.finish();

    qsizetype index = -1;
    for (qsizetype i = 0; i < order.size(); ++i) {
        if (order.at(i).first == gameId) {
            index = i;
            break;
        }
    }
    const qsizetype neighborIndex = index + direction;
    if (index < 0) {
        return fail(error, QStringLiteral("This game no longer exists in the library."));
    }
    if (neighborIndex < 0 || neighborIndex >= order.size()) {
        return true;
    }

    if (!connection.transaction()) {
        return fail(error, QStringLiteral("Could not reorder games: %1")
                               .arg(connection.lastError().text()));
    }
    QSqlQuery update(connection);
    update.prepare(QStringLiteral("UPDATE games SET custom_order = ? WHERE id = ?"));
    update.addBindValue(order.at(neighborIndex).second);
    update.addBindValue(gameId);
    if (!update.exec()) {
        connection.rollback();
        return fail(error, QStringLiteral("Could not reorder games: %1")
                               .arg(databaseError(update)));
    }
    update.bindValue(0, order.at(index).second);
    update.bindValue(1, order.at(neighborIndex).first);
    if (!update.exec()) {
        connection.rollback();
        return fail(error, QStringLiteral("Could not reorder games: %1")
                               .arg(databaseError(update)));
    }
    if (!connection.commit()) {
        connection.rollback();
        return fail(error, QStringLiteral("Could not save the custom order: %1")
                               .arg(connection.lastError().text()));
    }
    return true;
}

bool GameRepository::exportLibrary(QJsonDocument& document, QString* error) const
{
    if (error != nullptr) {
        error->clear();
    }
    QVector<Game> games;
    if (!list(games, error, GameSort::Custom)) {
        return false;
    }
    QJsonArray array;
    for (const Game& game : games) {
        array.append(gameToJson(game));
    }
    QJsonObject root;
    root.insert(QStringLiteral("schemaVersion"), 1);
    root.insert(QStringLiteral("games"), array);
    document = QJsonDocument(root);
    return true;
}

bool GameRepository::importLibrary(const QJsonDocument& document, int* importedCount,
                                  int* skippedCount, QString* error)
{
    if (error != nullptr) {
        error->clear();
    }
    if (!document.isObject() ||
        document.object().value(QStringLiteral("schemaVersion")).toInt() != 1 ||
        !document.object().value(QStringLiteral("games")).isArray()) {
        return fail(error, QStringLiteral("This is not a supported Nexus library export."));
    }

    QSqlDatabase connection = database_.connection();
    if (!connection.transaction()) {
        return fail(error, QStringLiteral("Could not start the library import: %1")
                               .arg(connection.lastError().text()));
    }
    QSqlQuery orderQuery(connection);
    if (!orderQuery.exec(QStringLiteral(
            "SELECT COALESCE(MAX(custom_order) + 1, 0) FROM games")) ||
        !orderQuery.next()) {
        connection.rollback();
        return fail(error, QStringLiteral("Could not prepare the imported game order."));
    }
    qint64 customOrder = orderQuery.value(0).toLongLong();
    orderQuery.finish();

    int imported = 0;
    int skipped = 0;
    const QJsonArray values = document.object().value(QStringLiteral("games")).toArray();
    for (const QJsonValue& value : values) {
        if (!value.isObject()) {
            connection.rollback();
            return fail(error, QStringLiteral("The library export contains an invalid game entry."));
        }
        Game game = gameFromJson(value.toObject());
        if (game.title.isEmpty() || game.executablePath.isEmpty()) {
            connection.rollback();
            return fail(error, QStringLiteral(
                                   "Every imported game needs a title and executable path."));
        }

        game.executablePath = QDir::toNativeSeparators(
            QDir::cleanPath(QFileInfo(game.executablePath).absoluteFilePath()));
        const QString key = normalizedExecutableKey(game.executablePath);
        QSqlQuery duplicateCheck(connection);
        duplicateCheck.prepare(QStringLiteral(
            "SELECT id FROM games WHERE executable_key = ?"));
        duplicateCheck.addBindValue(key);
        if (!duplicateCheck.exec()) {
            connection.rollback();
            return fail(error, QStringLiteral("Could not check imported paths: %1")
                                   .arg(databaseError(duplicateCheck)));
        }
        if (duplicateCheck.next()) {
            ++skipped;
            duplicateCheck.finish();
            continue;
        }
        duplicateCheck.finish();

        QJsonArray tags;
        for (const QString& tag : game.tags) {
            const QString normalizedTag = tag.trimmed();
            if (!normalizedTag.isEmpty()) {
                tags.append(normalizedTag);
            }
        }
        QSqlQuery insert(connection);
        insert.prepare(QStringLiteral(
            "INSERT INTO games (title, description, executable_path, executable_key, "
            "working_directory, launch_arguments, cover_path, launcher, genre, category, "
            "collection, franchise, tags_json, favorite, launch_count, total_play_seconds, "
            "last_played, date_added, custom_order) "
            "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)"));
        insert.addBindValue(game.title);
        insert.addBindValue(nonNullText(game.description));
        insert.addBindValue(game.executablePath);
        insert.addBindValue(key);
        insert.addBindValue(nonNullText(game.workingDirectory));
        insert.addBindValue(nonNullText(game.launchArguments));
        insert.addBindValue(nonNullText(game.coverPath));
        insert.addBindValue(game.launcher.isEmpty() ? QStringLiteral("Manual") : game.launcher);
        insert.addBindValue(nonNullText(game.genre));
        insert.addBindValue(nonNullText(game.category));
        insert.addBindValue(nonNullText(game.collection));
        insert.addBindValue(nonNullText(game.franchise));
        insert.addBindValue(QString::fromUtf8(
            QJsonDocument(tags).toJson(QJsonDocument::Compact)));
        insert.addBindValue(game.favorite);
        insert.addBindValue(game.launchCount);
        insert.addBindValue(game.totalPlaySeconds);
        insert.addBindValue(game.lastPlayed.isValid() ? isoDateTime(game.lastPlayed) : QVariant());
        insert.addBindValue(game.dateAdded.isValid()
                                ? isoDateTime(game.dateAdded)
                                : isoDateTime(QDateTime::currentDateTimeUtc()));
        insert.addBindValue(customOrder++);
        if (!insert.exec()) {
            connection.rollback();
            return fail(error, QStringLiteral("Could not import a game: %1")
                                   .arg(databaseError(insert)));
        }
        ++imported;
    }

    if (!connection.commit()) {
        connection.rollback();
        return fail(error, QStringLiteral("Could not finish the library import: %1")
                               .arg(connection.lastError().text()));
    }
    if (importedCount != nullptr) {
        *importedCount = imported;
    }
    if (skippedCount != nullptr) {
        *skippedCount = skipped;
    }
    return true;
}

bool GameRepository::beginSession(qint64 gameId, qint64* sessionId, QString* error)
{
    if (error != nullptr) {
        error->clear();
    }
    QSqlDatabase connection = database_.connection();
    if (!connection.transaction()) {
        return fail(error, QStringLiteral("Could not start a play session: %1")
                               .arg(connection.lastError().text()));
    }

    const QString startedAt = isoDateTime(QDateTime::currentDateTimeUtc());
    QSqlQuery updateGame(connection);
    updateGame.prepare(QStringLiteral(
        "UPDATE games SET launch_count = launch_count + 1, last_played = ? WHERE id = ?"));
    updateGame.addBindValue(startedAt);
    updateGame.addBindValue(gameId);
    if (!updateGame.exec()) {
        connection.rollback();
        return fail(error, QStringLiteral("Could not record the game launch: %1")
                               .arg(databaseError(updateGame)));
    }
    if (updateGame.numRowsAffected() == 0) {
        connection.rollback();
        return fail(error, QStringLiteral("This game no longer exists in the library."));
    }

    QSqlQuery insertSession(connection);
    insertSession.prepare(QStringLiteral(
        "INSERT INTO launch_sessions (game_id, started_at, status) VALUES (?, ?, 'running')"));
    insertSession.addBindValue(gameId);
    insertSession.addBindValue(startedAt);
    if (!insertSession.exec()) {
        connection.rollback();
        return fail(error, QStringLiteral("Could not create a play session: %1")
                               .arg(databaseError(insertSession)));
    }

    const qint64 createdId = insertSession.lastInsertId().toLongLong();
    if (!connection.commit()) {
        connection.rollback();
        return fail(error, QStringLiteral("Could not save the play session: %1")
                               .arg(connection.lastError().text()));
    }
    if (sessionId != nullptr) {
        *sessionId = createdId;
    }
    return true;
}

bool GameRepository::finishSession(qint64 sessionId, int exitCode, bool interrupted,
                                   QString* error)
{
    if (error != nullptr) {
        error->clear();
    }
    QSqlDatabase connection = database_.connection();
    if (!connection.transaction()) {
        return fail(error, QStringLiteral("Could not finish the play session: %1")
                               .arg(connection.lastError().text()));
    }

    QSqlQuery findSession(connection);
    findSession.prepare(QStringLiteral(
        "SELECT game_id, started_at, status FROM launch_sessions WHERE id = ?"));
    findSession.addBindValue(sessionId);
    if (!findSession.exec() || !findSession.next()) {
        connection.rollback();
        return fail(error, QStringLiteral("The play session could not be found."));
    }
    if (findSession.value(2).toString() != QStringLiteral("running")) {
        connection.commit();
        return true;
    }

    const qint64 gameId = findSession.value(0).toLongLong();
    const QDateTime startedAt = QDateTime::fromString(findSession.value(1).toString(),
                                                       Qt::ISODateWithMs);
    const qint64 seconds = qMax<qint64>(
        0, startedAt.secsTo(QDateTime::currentDateTimeUtc()));
    const QString endedAt = isoDateTime(QDateTime::currentDateTimeUtc());
    const QString status = interrupted ? QStringLiteral("interrupted")
                                       : QStringLiteral("completed");

    QSqlQuery updateSession(connection);
    updateSession.prepare(QStringLiteral(
        "UPDATE launch_sessions SET ended_at = ?, duration_seconds = ?, exit_code = ?, "
        "status = ? WHERE id = ?"));
    updateSession.addBindValue(endedAt);
    updateSession.addBindValue(seconds);
    updateSession.addBindValue(interrupted ? QVariant() : QVariant(exitCode));
    updateSession.addBindValue(status);
    updateSession.addBindValue(sessionId);
    if (!updateSession.exec()) {
        connection.rollback();
        return fail(error, QStringLiteral("Could not close the play session: %1")
                               .arg(databaseError(updateSession)));
    }

    QSqlQuery updateGame(connection);
    updateGame.prepare(QStringLiteral(
        "UPDATE games SET total_play_seconds = total_play_seconds + ? WHERE id = ?"));
    updateGame.addBindValue(seconds);
    updateGame.addBindValue(gameId);
    if (!updateGame.exec()) {
        connection.rollback();
        return fail(error, QStringLiteral("Could not update game playtime: %1")
                               .arg(databaseError(updateGame)));
    }
    if (!connection.commit()) {
        connection.rollback();
        return fail(error, QStringLiteral("Could not save the completed session: %1")
                               .arg(connection.lastError().text()));
    }
    return true;
}

bool GameRepository::recoverInterruptedSessions(QString* error)
{
    if (error != nullptr) {
        error->clear();
    }
    QSqlDatabase connection = database_.connection();
    QSqlQuery query(connection);
    if (!query.exec(QStringLiteral(
            "SELECT id FROM launch_sessions WHERE status = 'running'"))) {
        return fail(error, QStringLiteral("Could not check incomplete sessions: %1")
                               .arg(databaseError(query)));
    }

    QVector<qint64> pendingIds;
    while (query.next()) {
        pendingIds.append(query.value(0).toLongLong());
    }
    query.finish();
    for (const qint64 id : pendingIds) {
        if (!finishSession(id, -1, true, error)) {
            return false;
        }
    }
    return true;
}

QString GameRepository::normalizedExecutableKey(const QString& path)
{
    return QDir::cleanPath(QFileInfo(path).absoluteFilePath())
        .replace(QLatin1Char('\\'), QLatin1Char('/'))
        .toCaseFolded();
}

} // namespace nexus
