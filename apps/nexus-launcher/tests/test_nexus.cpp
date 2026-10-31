#include "nexus/app/app_settings.h"
#include "nexus/data/database.h"
#include "nexus/data/game_repository.h"
#include "nexus/launcher/game_launcher.h"

#include <QFile>
#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QtTest>

using namespace nexus;

class NexusTests final : public QObject {
    Q_OBJECT

private slots:
    void settingsRoundTrip();
    void malformedSettingsArePreserved();
    void libraryRejectsDuplicatesAndTracksSessions();
    void launchArgumentsRespectQuoting();
};

void NexusTests::settingsRoundTrip()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("settings.json"));

    SettingsStore first(path);
    QString warning;
    QVERIFY(first.load(&warning));
    QVERIFY(warning.isEmpty());
    QCOMPARE(first.settings().theme, QStringLiteral("dark"));

    AppSettings changed = first.settings();
    changed.theme = QStringLiteral("light");
    changed.accentColor = QStringLiteral("#14B8A6");
    changed.compactMode = true;
    changed.defaultLibraryView = QStringLiteral("list");
    QString error;
    QVERIFY2(first.save(changed, &error), qPrintable(error));

    SettingsStore second(path);
    QVERIFY2(second.load(&warning), qPrintable(warning));
    QCOMPARE(second.settings().theme, QStringLiteral("light"));
    QCOMPARE(second.settings().accentColor, QStringLiteral("#14b8a6"));
    QVERIFY(second.settings().compactMode);
    QCOMPARE(second.settings().defaultLibraryView, QStringLiteral("list"));
}

void NexusTests::malformedSettingsArePreserved()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("settings.json"));
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write("{not valid json");
    file.close();

    SettingsStore store(path);
    QString warning;
    QVERIFY(store.load(&warning));
    QVERIFY(!warning.isEmpty());
    QCOMPARE(store.settings().theme, QStringLiteral("dark"));
    QVERIFY(!QDir(directory.path()).entryList(
                 {QStringLiteral("settings.json.corrupt-*")}, QDir::Files).isEmpty());
}

void NexusTests::libraryRejectsDuplicatesAndTracksSessions()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString executablePath = directory.filePath(QStringLiteral("SampleGame.exe"));
    QFile executable(executablePath);
    QVERIFY(executable.open(QIODevice::WriteOnly));
    executable.write("local test fixture");
    executable.close();

    Database database(directory.filePath(QStringLiteral("test.sqlite3")));
    QString error;
    QVERIFY2(database.open(&error), qPrintable(error));
    GameRepository repository(database);

    Game game;
    game.title = QStringLiteral("Sample Game");
    game.executablePath = executablePath;
    game.launcher = QStringLiteral("Manual");
    game.genre = QStringLiteral("Adventure");
    game.category = QStringLiteral("Single player");
    game.collection = QStringLiteral("Weekend");
    game.franchise = QStringLiteral("Sample series");
    game.tags = {QStringLiteral("indie"), QStringLiteral("test")};
    qint64 gameId = 0;
    QVERIFY2(repository.add(game, &gameId, &error), qPrintable(error));
    QVERIFY(gameId > 0);

    Game duplicate = game;
    duplicate.title = QStringLiteral("Another Name");
    duplicate.executablePath = QDir(directory.path()).filePath(
        QStringLiteral("./SampleGame.exe"));
    QVERIFY(!repository.add(duplicate, nullptr, &error));
    QVERIFY(error.contains(QStringLiteral("already"), Qt::CaseInsensitive));

    Game missing = game;
    missing.title = QStringLiteral("Missing Path Game");
    missing.executablePath = directory.filePath(QStringLiteral("missing.exe"));
    QVERIFY(!repository.add(missing, nullptr, &error));
    QVERIFY(error.contains(QStringLiteral("executable"), Qt::CaseInsensitive));

    QVERIFY2(repository.setFavorite(gameId, true, &error), qPrintable(error));
    qint64 sessionId = 0;
    QVERIFY2(repository.beginSession(gameId, &sessionId, &error), qPrintable(error));
    QVERIFY(sessionId > 0);
    QTest::qWait(1100);
    QVERIFY2(repository.finishSession(sessionId, 0, false, &error), qPrintable(error));

    QVector<Game> games;
    QVERIFY2(repository.list(games, &error, GameSort::Playtime), qPrintable(error));
    QCOMPARE(games.size(), 1);
    QCOMPARE(games.first().id, gameId);
    QVERIFY(games.first().favorite);
    QCOMPARE(games.first().genre, QStringLiteral("Adventure"));
    QCOMPARE(games.first().category, QStringLiteral("Single player"));
    QCOMPARE(games.first().collection, QStringLiteral("Weekend"));
    QCOMPARE(games.first().franchise, QStringLiteral("Sample series"));
    QCOMPARE(games.first().launchCount, 1);
    QVERIFY(games.first().totalPlaySeconds >= 1);
    QVERIFY(games.first().lastPlayed.isValid());

    QJsonDocument exported;
    QVERIFY2(repository.exportLibrary(exported, &error), qPrintable(error));
    Database importedDatabase(directory.filePath(QStringLiteral("imported.sqlite3")));
    QVERIFY2(importedDatabase.open(&error), qPrintable(error));
    GameRepository importedRepository(importedDatabase);
    int importedCount = 0;
    int skippedCount = 0;
    QVERIFY2(importedRepository.importLibrary(exported, &importedCount,
                                              &skippedCount, &error),
             qPrintable(error));
    QCOMPARE(importedCount, 1);
    QCOMPARE(skippedCount, 0);
    QVERIFY2(importedRepository.importLibrary(exported, &importedCount,
                                              &skippedCount, &error),
             qPrintable(error));
    QCOMPARE(importedCount, 0);
    QCOMPARE(skippedCount, 1);
    QVERIFY2(importedRepository.list(games, &error), qPrintable(error));
    QCOMPARE(games.size(), 1);
    QCOMPARE(games.first().genre, QStringLiteral("Adventure"));

    const QString secondExecutablePath =
        directory.filePath(QStringLiteral("SecondGame.exe"));
    QFile secondExecutable(secondExecutablePath);
    QVERIFY(secondExecutable.open(QIODevice::WriteOnly));
    secondExecutable.write("second fixture");
    secondExecutable.close();
    Game secondGame;
    secondGame.title = QStringLiteral("Second Game");
    secondGame.executablePath = secondExecutablePath;
    qint64 secondGameId = 0;
    QVERIFY2(repository.add(secondGame, &secondGameId, &error), qPrintable(error));
    QVERIFY2(repository.list(games, &error, GameSort::Custom), qPrintable(error));
    QCOMPARE(games.size(), 2);
    QCOMPARE(games.first().id, gameId);
    QVERIFY2(repository.moveGame(secondGameId, -1, &error), qPrintable(error));
    QVERIFY2(repository.list(games, &error, GameSort::Custom), qPrintable(error));
    QCOMPARE(games.first().id, secondGameId);

    QVERIFY2(repository.remove(gameId, &error), qPrintable(error));
    QVERIFY2(repository.list(games, &error), qPrintable(error));
    QCOMPARE(games.size(), 1);
    QVERIFY2(repository.remove(secondGameId, &error), qPrintable(error));
    QVERIFY2(repository.list(games, &error), qPrintable(error));
    QVERIFY(games.isEmpty());
    QVERIFY(QFileInfo::exists(executablePath));
}

void NexusTests::launchArgumentsRespectQuoting()
{
    const QStringList parsed =
        GameLauncher::parseArguments(QStringLiteral("--windowed --name \"My Game\" -scale 1.2"));
    QCOMPARE(parsed,
             (QStringList{QStringLiteral("--windowed"), QStringLiteral("--name"),
                          QStringLiteral("My Game"), QStringLiteral("-scale"),
                          QStringLiteral("1.2")}));
}

QTEST_GUILESS_MAIN(NexusTests)

#include "test_nexus.moc"
