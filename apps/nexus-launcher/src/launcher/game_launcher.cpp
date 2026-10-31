#include "nexus/launcher/game_launcher.h"

#include <QDir>
#include <QFileInfo>
#include <QTimer>

#ifdef Q_OS_WIN
#define NOMINMAX
#include <windows.h>
#endif

namespace nexus {
namespace {

#ifdef Q_OS_WIN
struct WindowSearch {
    DWORD processId = 0;
    HWND window = nullptr;
};

BOOL CALLBACK findProcessWindow(HWND window, LPARAM userData)
{
    auto* search = reinterpret_cast<WindowSearch*>(userData);
    DWORD windowProcessId = 0;
    GetWindowThreadProcessId(window, &windowProcessId);
    if (windowProcessId == search->processId && IsWindowVisible(window)) {
        search->window = window;
        return FALSE;
    }
    return TRUE;
}
#endif

} // namespace

GameLauncher::GameLauncher(GameRepository& repository, QObject* parent)
    : QObject(parent),
      repository_(repository)
{
}

GameLauncher::~GameLauncher()
{
    // Closing Nexus must not close games that it started. Detach running
    // QProcess objects before QObject's base destructor cleans up its children.
    for (auto it = processes_.begin(); it != processes_.end(); ++it) {
        if (it->process != nullptr) {
            if (it->sessionId > 0) {
                QString error;
                if (!repository_.finishSession(it->sessionId, -1, true, &error)) {
                    qWarning().noquote() << QStringLiteral("Could not save session on exit: %1")
                                                .arg(error);
                }
            }
            disconnect(it->process, nullptr, this, nullptr);
            it->process->setParent(nullptr);
        }
    }
}

bool GameLauncher::isRunning(qint64 gameId) const
{
    const auto it = processes_.constFind(gameId);
    return it != processes_.cend() && it->process != nullptr &&
           it->process->state() == QProcess::Running;
}

bool GameLauncher::isStarting(qint64 gameId) const
{
    const auto it = processes_.constFind(gameId);
    return it != processes_.cend() && it->process != nullptr &&
           it->process->state() == QProcess::Starting;
}

bool GameLauncher::launch(const Game& game, QString* error)
{
    if (error != nullptr) {
        error->clear();
    }
    if (game.id <= 0) {
        if (error != nullptr) {
            *error = QStringLiteral("Add this game to your library before launching it.");
        }
        return false;
    }
    if (processes_.contains(game.id)) {
        const QProcess* active = processes_.value(game.id).process;
        if (active != nullptr && active->state() != QProcess::NotRunning) {
            if (error != nullptr) {
                *error = QStringLiteral("This game is already starting or running.");
            }
            return false;
        }
        processes_.remove(game.id);
    }

    const QFileInfo executable(game.executablePath);
    if (!executable.exists() || !executable.isFile()) {
        if (error != nullptr) {
            *error = QStringLiteral("The game executable is missing. Edit the game and repair its path.");
        }
        return false;
    }

    QString workingDirectory = game.workingDirectory.trimmed();
    if (workingDirectory.isEmpty()) {
        workingDirectory = executable.absolutePath();
    } else if (!QFileInfo(workingDirectory).isDir()) {
        if (error != nullptr) {
            *error = QStringLiteral("The configured working directory does not exist.");
        }
        return false;
    }

    auto* process = new QProcess(this);
    process->setWorkingDirectory(QDir::toNativeSeparators(workingDirectory));
    processes_.insert(game.id, RunningProcess{process, 0});

    connect(process, &QProcess::started, this, [this, gameId = game.id]() {
        auto it = processes_.find(gameId);
        if (it == processes_.end()) {
            return;
        }
        QString trackingMessage;
        qint64 sessionId = 0;
        if (repository_.beginSession(gameId, &sessionId, &trackingMessage)) {
            it->sessionId = sessionId;
        } else {
            emit trackingError(gameId, trackingMessage);
        }
        emit processStarted(gameId);
    });
    connect(process, &QProcess::finished, this,
            [this, gameId = game.id, process](int exitCode, QProcess::ExitStatus status) {
                finishProcess(gameId, process, exitCode, status);
            });
    connect(process, &QProcess::errorOccurred, this,
            [this, gameId = game.id, process](QProcess::ProcessError processError) {
                if (processError != QProcess::FailedToStart) {
                    return;
                }
                const auto it = processes_.find(gameId);
                if (it == processes_.end() || it->process != process) {
                    return;
                }
                const QString message = process->errorString();
                processes_.erase(it);
                process->deleteLater();
                emit launchFailed(gameId, message);
            });

    process->start(QDir::toNativeSeparators(executable.absoluteFilePath()),
                   parseArguments(game.launchArguments));
    return true;
}

bool GameLauncher::focus(qint64 gameId, QString* error)
{
    if (error != nullptr) {
        error->clear();
    }
    const auto it = processes_.constFind(gameId);
    if (it == processes_.cend() || it->process == nullptr ||
        it->process->state() != QProcess::Running) {
        if (error != nullptr) {
            *error = QStringLiteral("This game is not currently running.");
        }
        return false;
    }

#ifdef Q_OS_WIN
    const qint64 pid = it->process->processId();
    if (pid <= 0) {
        if (error != nullptr) {
            *error = QStringLiteral("Windows could not identify the game process.");
        }
        return false;
    }
    WindowSearch search{static_cast<DWORD>(pid), nullptr};
    EnumWindows(findProcessWindow, reinterpret_cast<LPARAM>(&search));
    if (search.window == nullptr) {
        if (error != nullptr) {
            *error = QStringLiteral("No visible game window was found.");
        }
        return false;
    }
    ShowWindow(search.window, SW_RESTORE);
    if (!SetForegroundWindow(search.window)) {
        if (error != nullptr) {
            *error = QStringLiteral("Windows did not allow the game window to take focus.");
        }
        return false;
    }
    return true;
#else
    if (error != nullptr) {
        *error = QStringLiteral("Focusing a running window is available on Windows.");
    }
    return false;
#endif
}

bool GameLauncher::stop(qint64 gameId, QString* error)
{
    if (error != nullptr) {
        error->clear();
    }
    const auto it = processes_.constFind(gameId);
    if (it == processes_.cend() || it->process == nullptr ||
        it->process->state() == QProcess::NotRunning) {
        if (error != nullptr) {
            *error = QStringLiteral("This game is not currently running.");
        }
        return false;
    }

    QProcess* process = it->process;
    process->terminate();
    QTimer::singleShot(3000, process, [process]() {
        if (process->state() != QProcess::NotRunning) {
            process->kill();
        }
    });
    return true;
}

QStringList GameLauncher::parseArguments(const QString& commandLine)
{
    return QProcess::splitCommand(commandLine);
}

void GameLauncher::finishProcess(qint64 gameId, QProcess* process, int exitCode,
                                 QProcess::ExitStatus status)
{
    const auto it = processes_.find(gameId);
    if (it == processes_.end() || it->process != process) {
        return;
    }

    const qint64 sessionId = it->sessionId;
    processes_.erase(it);
    if (sessionId > 0) {
        QString error;
        if (!repository_.finishSession(sessionId, exitCode,
                                       status == QProcess::CrashExit, &error)) {
            emit trackingError(gameId, error);
        }
    }
    process->deleteLater();
    emit processExited(gameId, exitCode, status == QProcess::CrashExit);
}

} // namespace nexus
