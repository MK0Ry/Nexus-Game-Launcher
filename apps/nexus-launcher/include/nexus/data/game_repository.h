#pragma once

#include "nexus/data/database.h"
#include "nexus/model/game.h"

#include <QJsonDocument>
#include <QVector>

namespace nexus {

enum class GameSort {
    Name,
    RecentlyPlayed,
    Playtime,
    DateAdded,
    Custom,
};

class GameRepository {
public:
    explicit GameRepository(Database& database);

    bool list(QVector<Game>& games, QString* error = nullptr,
              GameSort sort = GameSort::Name) const;
    bool add(Game game, qint64* newId = nullptr, QString* error = nullptr);
    bool update(const Game& game, QString* error = nullptr);
    bool remove(qint64 gameId, QString* error = nullptr);
    bool setFavorite(qint64 gameId, bool favorite, QString* error = nullptr);
    bool moveGame(qint64 gameId, int direction, QString* error = nullptr);
    bool exportLibrary(QJsonDocument& document, QString* error = nullptr) const;
    bool importLibrary(const QJsonDocument& document, int* importedCount = nullptr,
                       int* skippedCount = nullptr, QString* error = nullptr);
    bool beginSession(qint64 gameId, qint64* sessionId = nullptr,
                      QString* error = nullptr);
    bool finishSession(qint64 sessionId, int exitCode, bool interrupted = false,
                       QString* error = nullptr);
    bool recoverInterruptedSessions(QString* error = nullptr);

    [[nodiscard]] static QString normalizedExecutableKey(const QString& path);

private:
    Database& database_;
};

} // namespace nexus
