#pragma once

#include "nexus/data/game_repository.h"

#include <QHash>
#include <QObject>
#include <QProcess>
#include <QStringList>

namespace nexus {

class GameLauncher : public QObject {
    Q_OBJECT

public:
    explicit GameLauncher(GameRepository& repository, QObject* parent = nullptr);
    ~GameLauncher() override;

    [[nodiscard]] bool isRunning(qint64 gameId) const;
    [[nodiscard]] bool isStarting(qint64 gameId) const;
    bool launch(const Game& game, QString* error = nullptr);
    bool focus(qint64 gameId, QString* error = nullptr);
    bool stop(qint64 gameId, QString* error = nullptr);

    [[nodiscard]] static QStringList parseArguments(const QString& commandLine);

signals:
    void processStarted(qint64 gameId);
    void processExited(qint64 gameId, int exitCode, bool crashed);
    void launchFailed(qint64 gameId, const QString& message);
    void trackingError(qint64 gameId, const QString& message);

private:
    struct RunningProcess {
        QProcess* process = nullptr;
        qint64 sessionId = 0;
    };

    void finishProcess(qint64 gameId, QProcess* process, int exitCode,
                       QProcess::ExitStatus status);

    GameRepository& repository_;
    QHash<qint64, RunningProcess> processes_;
};

} // namespace nexus
