#pragma once

#include "nexus/app/app_settings.h"
#include "nexus/data/game_repository.h"
#include "nexus/launcher/game_launcher.h"

#include <QMainWindow>
#include <QVector>

class QCheckBox;
class QComboBox;
class QGridLayout;
class QLabel;
class QLineEdit;
class QPushButton;
class QResizeEvent;
class QScrollArea;
class QStackedWidget;
class QWidget;

namespace nexus {

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    MainWindow(SettingsStore& settings, GameRepository& repository,
               GameLauncher& launcher, QWidget* parent = nullptr);

protected:
    void resizeEvent(QResizeEvent* event) override;

private:
    enum class Page {
        Home,
        Library,
        Settings,
    };

    void buildUi();
    QWidget* buildHomePage();
    QWidget* buildLibraryPage();
    QWidget* buildSettingsPage();
    void navigate(Page page);
    void refreshAll();
    void refreshHome();
    void refreshLibrary();
    void applySettings();
    void openGameDialog(qint64 gameId = 0);
    void removeGame(const Game& game);
    QWidget* createGameCard(const Game& game, bool listMode);
    void addSelectedExecutable();
    void showError(const QString& title, const QString& details);
    [[nodiscard]] QVector<Game> currentGames(QString* error = nullptr) const;

    SettingsStore& settings_;
    GameRepository& repository_;
    GameLauncher& launcher_;
    Page currentPage_ = Page::Home;

    QStackedWidget* pageStack_ = nullptr;
    QLineEdit* searchEdit_ = nullptr;
    QLabel* statusLabel_ = nullptr;
    QLabel* homeSummaryLabel_ = nullptr;
    QLabel* homeFeaturedTitle_ = nullptr;
    QLabel* homeFeaturedSubtitle_ = nullptr;
    QPushButton* homeFeaturedButton_ = nullptr;
    QLabel* homeRecentlyPlayedLabel_ = nullptr;
    QLabel* homeFavoritesLabel_ = nullptr;
    QScrollArea* libraryScrollArea_ = nullptr;
    QWidget* libraryContents_ = nullptr;
    QGridLayout* libraryGrid_ = nullptr;
    QComboBox* libraryViewCombo_ = nullptr;
    QComboBox* sortCombo_ = nullptr;
    QComboBox* installedFilter_ = nullptr;
    QComboBox* genreFilter_ = nullptr;
    QComboBox* launcherFilter_ = nullptr;
    QComboBox* tagFilter_ = nullptr;
    QComboBox* themeCombo_ = nullptr;
    QComboBox* viewCombo_ = nullptr;
    QCheckBox* compactCheck_ = nullptr;
    QLabel* accentSwatch_ = nullptr;
    QVector<QPushButton*> navigationButtons_;
    bool rebuildingLibrary_ = false;
};

} // namespace nexus
