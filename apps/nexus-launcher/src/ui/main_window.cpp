#include "nexus/ui/main_window.h"

#include "nexus/ui/game_dialog.h"

#include <QApplication>
#include <QCheckBox>
#include <QColor>
#include <QColorDialog>
#include <QComboBox>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QLayoutItem>
#include <QMenu>
#include <QMessageBox>
#include <QPixmap>
#include <QPushButton>
#include <QSaveFile>
#include <QResizeEvent>
#include <QSignalBlocker>
#include <QScrollArea>
#include <QStackedWidget>
#include <QStandardPaths>
#include <QStyle>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>

#include <algorithm>

namespace nexus {
namespace {

QPushButton* button(const QString& text, const QString& objectName,
                    QWidget* parent = nullptr)
{
    auto* result = new QPushButton(text, parent);
    result->setObjectName(objectName);
    result->setCursor(Qt::PointingHandCursor);
    return result;
}

QLabel* label(const QString& text, const QString& objectName,
              QWidget* parent = nullptr)
{
    auto* result = new QLabel(text, parent);
    result->setObjectName(objectName);
    return result;
}

QString formattedPlaytime(qint64 totalSeconds)
{
    const qint64 hours = totalSeconds / 3600;
    const qint64 minutes = (totalSeconds % 3600) / 60;
    if (hours > 0) {
        return QStringLiteral("%1h %2m").arg(hours).arg(minutes);
    }
    return QStringLiteral("%1m").arg(minutes);
}

QString formattedLastPlayed(const QDateTime& value)
{
    if (!value.isValid()) {
        return QStringLiteral("Not played yet");
    }
    return value.toLocalTime().toString(QStringLiteral("MMM d, yyyy"));
}

QWidget* cardSurface(QWidget* parent = nullptr)
{
    auto* frame = new QFrame(parent);
    frame->setObjectName(QStringLiteral("surfaceCard"));
    frame->setFrameShape(QFrame::NoFrame);
    return frame;
}

} // namespace

MainWindow::MainWindow(SettingsStore& settings, GameRepository& repository,
                       GameLauncher& launcher, QWidget* parent)
    : QMainWindow(parent),
      settings_(settings),
      repository_(repository),
      launcher_(launcher)
{
    setWindowTitle(QStringLiteral("Nexus Game Launcher"));
    setMinimumSize(1060, 680);
    resize(1440, 900);
    buildUi();
    applySettings();
    refreshAll();

    connect(&launcher_, &GameLauncher::processStarted, this, [this](qint64) {
        refreshAll();
        if (statusLabel_ != nullptr) {
            statusLabel_->setText(QStringLiteral("Game started"));
        }
    });
    connect(&launcher_, &GameLauncher::processExited, this,
            [this](qint64, int, bool crashed) {
                refreshAll();
                if (statusLabel_ != nullptr) {
                    statusLabel_->setText(crashed ? QStringLiteral("Game closed unexpectedly")
                                                  : QStringLiteral("Session saved"));
                }
            });
    connect(&launcher_, &GameLauncher::launchFailed, this,
            [this](qint64, const QString& message) {
                showError(QStringLiteral("Could not start the game"), message);
                refreshAll();
            });
    connect(&launcher_, &GameLauncher::trackingError, this,
            [this](qint64, const QString& message) {
                showError(QStringLiteral("Playtime could not be saved"), message);
            });
}

void MainWindow::buildUi()
{
    auto* shell = new QWidget(this);
    shell->setObjectName(QStringLiteral("appShell"));
    auto* shellLayout = new QHBoxLayout(shell);
    shellLayout->setContentsMargins(0, 0, 0, 0);
    shellLayout->setSpacing(0);

    auto* sidebar = new QFrame(shell);
    sidebar->setObjectName(QStringLiteral("sidebar"));
    sidebar->setFixedWidth(232);
    auto* sidebarLayout = new QVBoxLayout(sidebar);
    sidebarLayout->setContentsMargins(18, 22, 16, 18);
    sidebarLayout->setSpacing(8);

    auto* brandRow = new QHBoxLayout();
    auto* brandMark = label(QStringLiteral("N"), QStringLiteral("brandMark"), sidebar);
    brandMark->setAlignment(Qt::AlignCenter);
    brandMark->setFixedSize(34, 34);
    auto* brandName = label(QStringLiteral("NEXUS"), QStringLiteral("brandName"), sidebar);
    auto* brandSubtitle = label(QStringLiteral("GAME LAUNCHER"),
                                QStringLiteral("brandSubtitle"), sidebar);
    auto* brandText = new QVBoxLayout();
    brandText->setSpacing(0);
    brandText->addWidget(brandName);
    brandText->addWidget(brandSubtitle);
    brandRow->addWidget(brandMark);
    brandRow->addSpacing(10);
    brandRow->addLayout(brandText);
    brandRow->addStretch();
    sidebarLayout->addLayout(brandRow);
    sidebarLayout->addSpacing(26);

    auto* sectionLabel = label(QStringLiteral("WORKSPACE"), QStringLiteral("sectionLabel"), sidebar);
    sidebarLayout->addWidget(sectionLabel);

    const auto addNavigation = [this, sidebarLayout, sidebar](const QString& text,
                                                               const QString& glyph,
                                                               int index) {
        auto* nav = button(glyph + QStringLiteral("    ") + text,
                           QStringLiteral("navButton"), sidebar);
        nav->setCheckable(true);
        nav->setProperty("pageIndex", index);
        navigationButtons_.append(nav);
        sidebarLayout->addWidget(nav);
        connect(nav, &QPushButton::clicked, this, [this, index]() {
            navigate(static_cast<Page>(index));
        });
    };
    addNavigation(QStringLiteral("Home"), QStringLiteral("⌂"), 0);
    addNavigation(QStringLiteral("Library"), QStringLiteral("▤"), 1);
    addNavigation(QStringLiteral("Settings"), QStringLiteral("⚙"), 2);
    sidebarLayout->addStretch();

    auto* localCard = cardSurface(sidebar);
    auto* localLayout = new QVBoxLayout(localCard);
    localLayout->setContentsMargins(13, 13, 13, 13);
    localLayout->setSpacing(5);
    localLayout->addWidget(label(QStringLiteral("LOCAL BY DESIGN"),
                                 QStringLiteral("localCardTitle"), localCard));
    localLayout->addWidget(label(QStringLiteral("Your library stays on this PC."),
                                 QStringLiteral("localCardCopy"), localCard));
    sidebarLayout->addWidget(localCard);
    sidebarLayout->addSpacing(8);
    auto* addGameNav = button(QStringLiteral("+    Add a game"),
                              QStringLiteral("primaryButton"), sidebar);
    addGameNav->setMinimumHeight(42);
    sidebarLayout->addWidget(addGameNav);
    connect(addGameNav, &QPushButton::clicked, this, &MainWindow::addSelectedExecutable);

    shellLayout->addWidget(sidebar);

    auto* content = new QWidget(shell);
    content->setObjectName(QStringLiteral("contentArea"));
    auto* contentLayout = new QVBoxLayout(content);
    contentLayout->setContentsMargins(30, 20, 30, 24);
    contentLayout->setSpacing(18);

    auto* topBar = new QHBoxLayout();
    topBar->setSpacing(14);
    auto* context = new QVBoxLayout();
    context->setSpacing(2);
    context->addWidget(label(QStringLiteral("YOUR PC GAMING HUB"),
                             QStringLiteral("eyebrow"), content));
    context->addWidget(label(QStringLiteral("Welcome back"),
                             QStringLiteral("topBarTitle"), content));
    topBar->addLayout(context);
    topBar->addStretch();

    searchEdit_ = new QLineEdit(content);
    searchEdit_->setObjectName(QStringLiteral("globalSearch"));
    searchEdit_->setPlaceholderText(QStringLiteral("Search games, tags, launchers"));
    searchEdit_->setClearButtonEnabled(true);
    searchEdit_->setMinimumWidth(300);
    searchEdit_->setMaximumWidth(430);
    topBar->addWidget(searchEdit_);

    statusLabel_ = label(QStringLiteral("Offline ready"),
                         QStringLiteral("statusPill"), content);
    statusLabel_->setAlignment(Qt::AlignCenter);
    topBar->addWidget(statusLabel_);
    contentLayout->addLayout(topBar);

    pageStack_ = new QStackedWidget(content);
    pageStack_->setObjectName(QStringLiteral("pageStack"));
    pageStack_->addWidget(buildHomePage());
    pageStack_->addWidget(buildLibraryPage());
    pageStack_->addWidget(buildSettingsPage());
    contentLayout->addWidget(pageStack_, 1);
    shellLayout->addWidget(content, 1);
    setCentralWidget(shell);

    connect(searchEdit_, &QLineEdit::textChanged, this, [this]() {
        if (currentPage_ == Page::Library) {
            refreshLibrary();
        }
    });
    if (!navigationButtons_.isEmpty()) {
        navigationButtons_.first()->setChecked(true);
        navigationButtons_.first()->setProperty("active", true);
    }
}

QWidget* MainWindow::buildHomePage()
{
    auto* page = new QWidget(this);
    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(18);

    auto* hero = new QFrame(page);
    hero->setObjectName(QStringLiteral("heroPanel"));
    hero->setMinimumHeight(210);
    auto* heroLayout = new QHBoxLayout(hero);
    heroLayout->setContentsMargins(28, 25, 28, 25);
    auto* heroText = new QVBoxLayout();
    heroText->setSpacing(8);
    heroText->addWidget(label(QStringLiteral("READY WHEN YOU ARE"),
                              QStringLiteral("heroEyebrow"), hero));
    homeFeaturedTitle_ = label(QStringLiteral("Your next session starts here"),
                               QStringLiteral("heroTitle"), hero);
    homeFeaturedSubtitle_ = label(
        QStringLiteral("Build a personal library and launch your games from one place."),
        QStringLiteral("heroSubtitle"), hero);
    homeFeaturedSubtitle_->setWordWrap(true);
    heroText->addWidget(homeFeaturedTitle_);
    heroText->addWidget(homeFeaturedSubtitle_);
    heroText->addStretch();
    homeFeaturedButton_ = button(QStringLiteral("Add your first game"),
                                 QStringLiteral("heroButton"), hero);
    homeFeaturedButton_->setMinimumHeight(42);
    connect(homeFeaturedButton_, &QPushButton::clicked, this,
            [this]() {
                const QVector<Game> games = currentGames();
                if (games.isEmpty()) {
                    addSelectedExecutable();
                    return;
                }
                const Game* selected = nullptr;
                for (const Game& game : games) {
                    if (game.lastPlayed.isValid()) {
                        selected = &game;
                        break;
                    }
                }
                if (selected == nullptr) {
                    navigate(Page::Library);
                    return;
                }
                QString error;
                const bool succeeded = launcher_.isRunning(selected->id)
                                           ? launcher_.focus(selected->id, &error)
                                           : launcher_.launch(*selected, &error);
                if (!succeeded) {
                    showError(QStringLiteral("Could not start the game"), error);
                }
            });
    heroLayout->addLayout(heroText, 1);
    heroLayout->addSpacing(20);
    heroLayout->addWidget(homeFeaturedButton_, 0, Qt::AlignBottom);
    layout->addWidget(hero);

    auto* summaryCard = cardSurface(page);
    auto* summaryLayout = new QVBoxLayout(summaryCard);
    summaryLayout->setContentsMargins(20, 16, 20, 16);
    summaryLayout->setSpacing(8);
    summaryLayout->addWidget(label(QStringLiteral("AT A GLANCE"),
                                  QStringLiteral("sectionLabel"), summaryCard));
    homeSummaryLabel_ = label(QStringLiteral("Loading your library…"),
                              QStringLiteral("summaryText"), summaryCard);
    summaryLayout->addWidget(homeSummaryLabel_);
    layout->addWidget(summaryCard);

    auto* recentHeader = new QHBoxLayout();
    auto* recentTitle = label(QStringLiteral("Recently played"),
                              QStringLiteral("sectionTitle"), page);
    recentHeader->addWidget(recentTitle);
    recentHeader->addStretch();
    auto* viewLibrary = button(QStringLiteral("Open library"),
                               QStringLiteral("textButton"), page);
    recentHeader->addWidget(viewLibrary);
    connect(viewLibrary, &QPushButton::clicked, this, [this]() {
        navigate(Page::Library);
    });
    layout->addLayout(recentHeader);

    auto* recentCard = cardSurface(page);
    auto* recentLayout = new QVBoxLayout(recentCard);
    recentLayout->setContentsMargins(18, 14, 18, 14);
    homeRecentlyPlayedLabel_ = label(
        QStringLiteral("Your recent sessions will appear here."),
        QStringLiteral("recentList"), recentCard);
    homeRecentlyPlayedLabel_->setWordWrap(true);
    recentLayout->addWidget(homeRecentlyPlayedLabel_);
    layout->addWidget(recentCard);

    layout->addWidget(label(QStringLiteral("Favorites"),
                            QStringLiteral("sectionTitle"), page));
    auto* favoritesCard = cardSurface(page);
    auto* favoritesLayout = new QVBoxLayout(favoritesCard);
    favoritesLayout->setContentsMargins(18, 14, 18, 14);
    homeFavoritesLabel_ = label(QStringLiteral("Favorite games will appear here."),
                                QStringLiteral("recentList"), favoritesCard);
    homeFavoritesLabel_->setWordWrap(true);
    favoritesLayout->addWidget(homeFavoritesLabel_);
    layout->addWidget(favoritesCard);
    layout->addStretch();
    return page;
}

QWidget* MainWindow::buildLibraryPage()
{
    auto* page = new QWidget(this);
    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(14);

    auto* titleRow = new QHBoxLayout();
    auto* titleGroup = new QVBoxLayout();
    titleGroup->setSpacing(3);
    titleGroup->addWidget(label(QStringLiteral("YOUR COLLECTION"),
                                QStringLiteral("eyebrow"), page));
    titleGroup->addWidget(label(QStringLiteral("Game library"),
                                QStringLiteral("pageTitle"), page));
    titleRow->addLayout(titleGroup);
    titleRow->addStretch();
    auto* importButton = button(QStringLiteral("Import"),
                                QStringLiteral("secondaryButton"), page);
    auto* exportButton = button(QStringLiteral("Export"),
                                QStringLiteral("secondaryButton"), page);
    titleRow->addWidget(importButton);
    titleRow->addWidget(exportButton);
    auto* addButton = button(QStringLiteral("+  Add game"),
                             QStringLiteral("primaryButton"), page);
    addButton->setMinimumHeight(40);
    titleRow->addWidget(addButton);
    connect(addButton, &QPushButton::clicked, this, &MainWindow::addSelectedExecutable);
    connect(exportButton, &QPushButton::clicked, this, [this]() {
        const QString path = QFileDialog::getSaveFileName(
            this, QStringLiteral("Export Nexus library"), QStringLiteral("nexus-library.json"),
            QStringLiteral("Nexus library (*.json)"));
        if (path.isEmpty()) {
            return;
        }
        QJsonDocument document;
        QString error;
        if (!repository_.exportLibrary(document, &error)) {
            showError(QStringLiteral("Could not export library"), error);
            return;
        }
        QSaveFile file(path);
        const QByteArray bytes = document.toJson(QJsonDocument::Indented);
        if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size() ||
            !file.commit()) {
            showError(QStringLiteral("Could not export library"),
                      file.errorString().isEmpty()
                          ? QStringLiteral("The export file could not be written.")
                          : file.errorString());
            return;
        }
        statusLabel_->setText(QStringLiteral("Library exported"));
    });
    connect(importButton, &QPushButton::clicked, this, [this]() {
        const QString path = QFileDialog::getOpenFileName(
            this, QStringLiteral("Import Nexus library"), QString(),
            QStringLiteral("Nexus library (*.json);;JSON files (*.json)"));
        if (path.isEmpty()) {
            return;
        }
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly)) {
            showError(QStringLiteral("Could not read library file"), file.errorString());
            return;
        }
        QJsonParseError parseError;
        const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
        if (parseError.error != QJsonParseError::NoError) {
            showError(QStringLiteral("Invalid library file"),
                      parseError.errorString());
            return;
        }
        int imported = 0;
        int skipped = 0;
        QString error;
        if (!repository_.importLibrary(document, &imported, &skipped, &error)) {
            showError(QStringLiteral("Could not import library"), error);
            return;
        }
        refreshAll();
        QMessageBox::information(
            this, QStringLiteral("Import complete"),
            QStringLiteral("%1 games imported. %2 duplicates skipped. Missing executable paths "
                           "were kept so you can repair them later.")
                .arg(imported)
                .arg(skipped));
    });
    layout->addLayout(titleRow);

    auto* toolbar = new QHBoxLayout();
    auto* libraryCount = label(QStringLiteral("All games"),
                               QStringLiteral("toolbarLabel"), page);
    toolbar->addWidget(libraryCount);
    toolbar->addStretch();
    sortCombo_ = new QComboBox(page);
    sortCombo_->setObjectName(QStringLiteral("sortCombo"));
    sortCombo_->addItem(QStringLiteral("Name"), static_cast<int>(GameSort::Name));
    sortCombo_->addItem(QStringLiteral("Recently played"),
                        static_cast<int>(GameSort::RecentlyPlayed));
    sortCombo_->addItem(QStringLiteral("Most played"),
                        static_cast<int>(GameSort::Playtime));
    sortCombo_->addItem(QStringLiteral("Date added"),
                        static_cast<int>(GameSort::DateAdded));
    sortCombo_->addItem(QStringLiteral("Custom order"),
                        static_cast<int>(GameSort::Custom));
    toolbar->addWidget(sortCombo_);

    libraryViewCombo_ = new QComboBox(page);
    libraryViewCombo_->setObjectName(QStringLiteral("libraryViewCombo"));
    libraryViewCombo_->addItem(QStringLiteral("Grid"), QStringLiteral("grid"));
    libraryViewCombo_->addItem(QStringLiteral("List"), QStringLiteral("list"));
    libraryViewCombo_->setCurrentIndex(settings_.settings().defaultLibraryView ==
                                               QStringLiteral("list")
                                           ? 1
                                           : 0);
    toolbar->addWidget(libraryViewCombo_);
    layout->addLayout(toolbar);

    auto* filterRow = new QHBoxLayout();
    installedFilter_ = new QComboBox(page);
    installedFilter_->addItem(QStringLiteral("All paths"), QString());
    installedFilter_->addItem(QStringLiteral("Available"), QStringLiteral("available"));
    installedFilter_->addItem(QStringLiteral("Missing"), QStringLiteral("missing"));
    genreFilter_ = new QComboBox(page);
    genreFilter_->addItem(QStringLiteral("All genres"), QString());
    launcherFilter_ = new QComboBox(page);
    launcherFilter_->addItem(QStringLiteral("All launchers"), QString());
    tagFilter_ = new QComboBox(page);
    tagFilter_->addItem(QStringLiteral("All tags"), QString());
    filterRow->addWidget(installedFilter_);
    filterRow->addWidget(genreFilter_);
    filterRow->addWidget(launcherFilter_);
    filterRow->addWidget(tagFilter_);
    filterRow->addStretch();
    layout->addLayout(filterRow);

    libraryScrollArea_ = new QScrollArea(page);
    libraryScrollArea_->setObjectName(QStringLiteral("libraryScrollArea"));
    libraryScrollArea_->setWidgetResizable(true);
    libraryScrollArea_->setFrameShape(QFrame::NoFrame);
    libraryContents_ = new QWidget(libraryScrollArea_);
    libraryGrid_ = new QGridLayout(libraryContents_);
    libraryGrid_->setContentsMargins(0, 0, 8, 0);
    libraryGrid_->setHorizontalSpacing(14);
    libraryGrid_->setVerticalSpacing(14);
    libraryGrid_->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    libraryScrollArea_->setWidget(libraryContents_);
    layout->addWidget(libraryScrollArea_, 1);

    connect(sortCombo_, qOverload<int>(&QComboBox::currentIndexChanged),
            this, &MainWindow::refreshLibrary);
    connect(installedFilter_, qOverload<int>(&QComboBox::currentIndexChanged),
            this, &MainWindow::refreshLibrary);
    connect(genreFilter_, qOverload<int>(&QComboBox::currentIndexChanged),
            this, &MainWindow::refreshLibrary);
    connect(launcherFilter_, qOverload<int>(&QComboBox::currentIndexChanged),
            this, &MainWindow::refreshLibrary);
    connect(tagFilter_, qOverload<int>(&QComboBox::currentIndexChanged),
            this, &MainWindow::refreshLibrary);
    connect(libraryViewCombo_, qOverload<int>(&QComboBox::currentIndexChanged),
            this, [this](int index) {
                AppSettings next = settings_.settings();
                next.defaultLibraryView = libraryViewCombo_->itemData(index).toString();
                QString error;
                if (!settings_.save(next, &error)) {
                    showError(QStringLiteral("Could not save settings"), error);
                    return;
                }
                if (viewCombo_ != nullptr) {
                    const QSignalBlocker blocker(viewCombo_);
                    viewCombo_->setCurrentIndex(viewCombo_->findData(next.defaultLibraryView));
                }
                applySettings();
                refreshLibrary();
            });

    return page;
}

QWidget* MainWindow::buildSettingsPage()
{
    auto* page = new QWidget(this);
    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(16);
    layout->addWidget(label(QStringLiteral("PREFERENCES"),
                            QStringLiteral("eyebrow"), page));
    layout->addWidget(label(QStringLiteral("Settings"),
                            QStringLiteral("pageTitle"), page));

    auto* appearanceCard = cardSurface(page);
    auto* appearanceLayout = new QVBoxLayout(appearanceCard);
    appearanceLayout->setContentsMargins(20, 18, 20, 18);
    appearanceLayout->setSpacing(14);
    appearanceLayout->addWidget(label(QStringLiteral("Appearance"),
                                      QStringLiteral("sectionTitle"), appearanceCard));

    auto* themeRow = new QHBoxLayout();
    themeRow->addWidget(label(QStringLiteral("Theme"),
                              QStringLiteral("settingLabel"), appearanceCard));
    themeRow->addStretch();
    themeCombo_ = new QComboBox(appearanceCard);
    themeCombo_->addItem(QStringLiteral("Dark"), QStringLiteral("dark"));
    themeCombo_->addItem(QStringLiteral("Light"), QStringLiteral("light"));
    themeCombo_->setCurrentIndex(settings_.settings().theme == QStringLiteral("light") ? 1 : 0);
    themeRow->addWidget(themeCombo_);
    appearanceLayout->addLayout(themeRow);

    auto* accentRow = new QHBoxLayout();
    accentRow->addWidget(label(QStringLiteral("Accent color"),
                               QStringLiteral("settingLabel"), appearanceCard));
    accentRow->addStretch();
    accentSwatch_ = label(QStringLiteral("   "),
                          QStringLiteral("accentSwatch"), appearanceCard);
    accentSwatch_->setFixedSize(32, 24);
    accentRow->addWidget(accentSwatch_);
    auto* accentButton = button(QStringLiteral("Choose color"),
                                QStringLiteral("secondaryButton"), appearanceCard);
    accentRow->addWidget(accentButton);
    appearanceLayout->addLayout(accentRow);

    compactCheck_ = new QCheckBox(QStringLiteral("Use compact spacing"),
                                  appearanceCard);
    compactCheck_->setChecked(settings_.settings().compactMode);
    appearanceLayout->addWidget(compactCheck_);
    auto* viewRow = new QHBoxLayout();
    viewRow->addWidget(label(QStringLiteral("Default library view"),
                             QStringLiteral("settingLabel"), appearanceCard));
    viewRow->addStretch();
    viewCombo_ = new QComboBox(appearanceCard);
    viewCombo_->addItem(QStringLiteral("Grid"), QStringLiteral("grid"));
    viewCombo_->addItem(QStringLiteral("List"), QStringLiteral("list"));
    viewCombo_->setCurrentIndex(settings_.settings().defaultLibraryView ==
                                        QStringLiteral("list")
                                    ? 1
                                    : 0);
    viewRow->addWidget(viewCombo_);
    appearanceLayout->addLayout(viewRow);
    layout->addWidget(appearanceCard);

    auto* dataCard = cardSurface(page);
    auto* dataLayout = new QVBoxLayout(dataCard);
    dataLayout->setContentsMargins(20, 18, 20, 18);
    dataLayout->setSpacing(8);
    dataLayout->addWidget(label(QStringLiteral("Local data"),
                                QStringLiteral("sectionTitle"), dataCard));
    const QString dataPath =
        QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    auto* pathLabel = label(QStringLiteral("Library database and session history\n%1")
                                .arg(QDir::toNativeSeparators(dataPath)),
                            QStringLiteral("mutedText"), dataCard);
    pathLabel->setWordWrap(true);
    dataLayout->addWidget(pathLabel);
    dataLayout->addWidget(label(QStringLiteral("Nexus does not require an account or send telemetry."),
                                QStringLiteral("mutedText"), dataCard));
    layout->addWidget(dataCard);

    auto* resetRow = new QHBoxLayout();
    resetRow->addStretch();
    auto* resetButton = button(QStringLiteral("Reset settings"),
                               QStringLiteral("dangerButton"), page);
    resetRow->addWidget(resetButton);
    layout->addLayout(resetRow);
    layout->addStretch();

    const auto saveAppearance = [this]() {
        AppSettings next = settings_.settings();
        next.theme = themeCombo_->currentData().toString();
        next.defaultLibraryView = viewCombo_->currentData().toString();
        next.compactMode = compactCheck_->isChecked();
        QString error;
        if (!settings_.save(next, &error)) {
            showError(QStringLiteral("Could not save settings"), error);
            return;
        }
        applySettings();
        refreshLibrary();
    };
    connect(themeCombo_, qOverload<int>(&QComboBox::currentIndexChanged),
            this, [saveAppearance](int) { saveAppearance(); });
    connect(viewCombo_, qOverload<int>(&QComboBox::currentIndexChanged),
            this, [saveAppearance](int) { saveAppearance(); });
    connect(compactCheck_, &QCheckBox::toggled, this,
            [saveAppearance](bool) { saveAppearance(); });
    connect(accentButton, &QPushButton::clicked, this, [this]() {
        const QColor current(settings_.settings().accentColor);
        const QColor chosen = QColorDialog::getColor(
            current, this, QStringLiteral("Choose Nexus accent color"),
            QColorDialog::ShowAlphaChannel);
        if (!chosen.isValid()) {
            return;
        }
        AppSettings next = settings_.settings();
        next.accentColor = chosen.name(QColor::HexRgb);
        QString error;
        if (!settings_.save(next, &error)) {
            showError(QStringLiteral("Could not save settings"), error);
            return;
        }
        applySettings();
    });
    connect(resetButton, &QPushButton::clicked, this, [this]() {
        const auto answer = QMessageBox::question(
            this, QStringLiteral("Reset appearance settings"),
            QStringLiteral("Restore the default theme, accent, spacing, and library view? "
                           "Your game library will not be changed."));
        if (answer != QMessageBox::Yes) {
            return;
        }
        QString error;
        if (!settings_.reset(&error)) {
            showError(QStringLiteral("Could not reset settings"), error);
            return;
        }
        const QSignalBlocker themeBlocker(themeCombo_);
        const QSignalBlocker viewBlocker(viewCombo_);
        const QSignalBlocker compactBlocker(compactCheck_);
        themeCombo_->setCurrentIndex(0);
        viewCombo_->setCurrentIndex(0);
        compactCheck_->setChecked(false);
        applySettings();
        refreshLibrary();
    });
    return page;
}

void MainWindow::navigate(Page page)
{
    currentPage_ = page;
    pageStack_->setCurrentIndex(static_cast<int>(page));
    for (QPushButton* nav : navigationButtons_) {
        const bool active = nav->property("pageIndex").toInt() == static_cast<int>(page);
        nav->setChecked(active);
        nav->setProperty("active", active);
        nav->style()->unpolish(nav);
        nav->style()->polish(nav);
    }
    if (page == Page::Library) {
        refreshLibrary();
    } else if (page == Page::Home) {
        refreshHome();
    }
}

void MainWindow::refreshAll()
{
    refreshHome();
    refreshLibrary();
}

void MainWindow::refreshHome()
{
    QString error;
    QVector<Game> games;
    if (!repository_.list(games, &error, GameSort::RecentlyPlayed)) {
        if (homeSummaryLabel_ != nullptr) {
            homeSummaryLabel_->setText(QStringLiteral("Library unavailable: %1").arg(error));
        }
        return;
    }

    qint64 totalPlaySeconds = 0;
    qint64 totalLaunches = 0;
    for (const Game& game : games) {
        totalPlaySeconds += game.totalPlaySeconds;
        totalLaunches += game.launchCount;
    }
    if (homeSummaryLabel_ != nullptr) {
        homeSummaryLabel_->setText(
            QStringLiteral("%1 games     ·     %2 total playtime     ·     %3 launches")
                .arg(games.size())
                .arg(formattedPlaytime(totalPlaySeconds))
                .arg(totalLaunches));
    }

    QVector<Game> recentGames;
    for (const Game& game : games) {
        if (game.lastPlayed.isValid()) {
            recentGames.append(game);
        }
    }
    if (homeRecentlyPlayedLabel_ != nullptr) {
        if (recentGames.isEmpty()) {
            homeRecentlyPlayedLabel_->setText(
                games.isEmpty()
                    ? QStringLiteral("Your library is empty. Add a game to get started.")
                    : QStringLiteral("No completed play sessions yet. Launch a game to begin tracking."));
        } else {
            QStringList rows;
            const int rowCount = qMin(5, recentGames.size());
            for (int i = 0; i < rowCount; ++i) {
                const Game& game = recentGames.at(i);
                rows.append(QStringLiteral("%1     ·     %2     ·     %3")
                                .arg(game.title,
                                     formattedLastPlayed(game.lastPlayed),
                                     formattedPlaytime(game.totalPlaySeconds)));
            }
            homeRecentlyPlayedLabel_->setText(rows.join(QLatin1Char('\n')));
        }
    }
    if (homeFavoritesLabel_ != nullptr) {
        QStringList favorites;
        for (const Game& game : games) {
            if (game.favorite) {
                favorites.append(game.title);
            }
        }
        homeFavoritesLabel_->setText(
            favorites.isEmpty()
                ? QStringLiteral("Mark a game as favorite to pin it here.")
                : favorites.mid(0, 8).join(QStringLiteral("     ·     ")));
    }

    if (homeFeaturedTitle_ == nullptr || homeFeaturedButton_ == nullptr) {
        return;
    }
    const Game* featured = nullptr;
    for (const Game& game : games) {
        if (game.lastPlayed.isValid()) {
            featured = &game;
            break;
        }
    }
    if (featured != nullptr) {
        homeFeaturedTitle_->setText(featured->title);
        homeFeaturedSubtitle_->setText(
            QStringLiteral("Last played %1  ·  %2 total")
                .arg(formattedLastPlayed(featured->lastPlayed),
                     formattedPlaytime(featured->totalPlaySeconds)));
        homeFeaturedButton_->setText(
            launcher_.isRunning(featured->id) ? QStringLiteral("Focus game")
                                              : (launcher_.isStarting(featured->id)
                                                     ? QStringLiteral("Starting…")
                                                     : QStringLiteral("Play now")));
        homeFeaturedButton_->setEnabled(!launcher_.isStarting(featured->id));
        homeFeaturedButton_->setVisible(true);
    } else if (!games.isEmpty()) {
        homeFeaturedTitle_->setText(QStringLiteral("Your library is ready"));
        homeFeaturedSubtitle_->setText(
            QStringLiteral("%1 games added. Pick one from your library and start a session.")
                .arg(games.size()));
        homeFeaturedButton_->setText(QStringLiteral("Open library"));
        homeFeaturedButton_->setEnabled(true);
    } else {
        homeFeaturedTitle_->setText(QStringLiteral("Your next session starts here"));
        homeFeaturedSubtitle_->setText(
            QStringLiteral("Build a personal library and launch your games from one place."));
        homeFeaturedButton_->setText(QStringLiteral("Add your first game"));
        homeFeaturedButton_->setEnabled(true);
    }
}

void MainWindow::refreshLibrary()
{
    if (libraryGrid_ == nullptr || rebuildingLibrary_) {
        return;
    }
    rebuildingLibrary_ = true;

    while (QLayoutItem* item = libraryGrid_->takeAt(0)) {
        if (item->widget() != nullptr) {
            item->widget()->deleteLater();
        }
        delete item;
    }

    QString error;
    QVector<Game> games = currentGames(&error);
    if (!error.isEmpty()) {
        auto* errorLabel = label(QStringLiteral("Could not load your games: %1").arg(error),
                                 QStringLiteral("errorText"), libraryContents_);
        libraryGrid_->addWidget(errorLabel, 0, 0);
        rebuildingLibrary_ = false;
        return;
    }

    const auto populateFilter = [](QComboBox* combo, const QString& allLabel,
                                   QStringList values) {
        if (combo == nullptr) {
            return;
        }
        values.removeDuplicates();
        values.sort(Qt::CaseInsensitive);
        const QString previous = combo->currentData().toString();
        const QSignalBlocker blocker(combo);
        combo->clear();
        combo->addItem(allLabel, QString());
        for (const QString& value : values) {
            combo->addItem(value, value);
        }
        const int previousIndex = combo->findData(previous);
        combo->setCurrentIndex(previousIndex < 0 ? 0 : previousIndex);
    };
    QStringList genres;
    QStringList launchers;
    QStringList tags;
    for (const Game& game : games) {
        if (!game.genre.trimmed().isEmpty()) {
            genres.append(game.genre.trimmed());
        }
        if (!game.launcher.trimmed().isEmpty()) {
            launchers.append(game.launcher.trimmed());
        }
        tags.append(game.tags);
    }
    populateFilter(genreFilter_, QStringLiteral("All genres"), genres);
    populateFilter(launcherFilter_, QStringLiteral("All launchers"), launchers);
    populateFilter(tagFilter_, QStringLiteral("All tags"), tags);

    const QString search = searchEdit_ == nullptr ? QString() : searchEdit_->text().trimmed();
    const QString availability =
        installedFilter_ == nullptr ? QString() : installedFilter_->currentData().toString();
    const QString genre = genreFilter_ == nullptr ? QString() : genreFilter_->currentData().toString();
    const QString launcher =
        launcherFilter_ == nullptr ? QString() : launcherFilter_->currentData().toString();
    const QString tag = tagFilter_ == nullptr ? QString() : tagFilter_->currentData().toString();
    if (!search.isEmpty() || !availability.isEmpty() || !genre.isEmpty() ||
        !launcher.isEmpty() || !tag.isEmpty()) {
        games.erase(std::remove_if(games.begin(), games.end(),
                                   [&search, &availability, &genre, &launcher, &tag](const Game& game) {
            if (availability == QStringLiteral("available") && !game.executableExists()) {
                return true;
            }
            if (availability == QStringLiteral("missing") && game.executableExists()) {
                return true;
            }
            if (!genre.isEmpty() && game.genre.compare(genre, Qt::CaseInsensitive) != 0) {
                return true;
            }
            if (!launcher.isEmpty() &&
                game.launcher.compare(launcher, Qt::CaseInsensitive) != 0) {
                return true;
            }
            if (!tag.isEmpty() && !game.tags.contains(tag, Qt::CaseInsensitive)) {
                return true;
            }
            if (search.isEmpty()) {
                return false;
            }
            if (game.title.contains(search, Qt::CaseInsensitive) ||
                game.description.contains(search, Qt::CaseInsensitive) ||
                game.launcher.contains(search, Qt::CaseInsensitive) ||
                game.genre.contains(search, Qt::CaseInsensitive) ||
                game.category.contains(search, Qt::CaseInsensitive) ||
                game.collection.contains(search, Qt::CaseInsensitive) ||
                game.franchise.contains(search, Qt::CaseInsensitive)) {
                return false;
            }
            for (const QString& tag : game.tags) {
                if (tag.contains(search, Qt::CaseInsensitive)) {
                    return false;
                }
            }
            return true;
        }), games.end());
    }

    const bool listMode = settings_.settings().defaultLibraryView == QStringLiteral("list");
    if (games.isEmpty()) {
        auto* empty = cardSurface(libraryContents_);
        empty->setMinimumHeight(190);
        auto* emptyLayout = new QVBoxLayout(empty);
        emptyLayout->setContentsMargins(25, 24, 25, 24);
        emptyLayout->setSpacing(10);
        const bool noGames = search.isEmpty() && availability.isEmpty() && genre.isEmpty() &&
                             launcher.isEmpty() && tag.isEmpty();
        emptyLayout->addWidget(label(noGames ? QStringLiteral("Your library is empty")
                                             : QStringLiteral("No matching games"),
                                     QStringLiteral("emptyTitle"), empty));
        auto* message = label(
            noGames ? QStringLiteral("Add an installed game to start building your collection.")
                    : QStringLiteral("Try another title, tag, or launcher name."),
            QStringLiteral("mutedText"), empty);
        emptyLayout->addWidget(message);
        if (noGames) {
            auto* addButton = button(QStringLiteral("Choose an executable"),
                                     QStringLiteral("primaryButton"), empty);
            addButton->setMaximumWidth(220);
            emptyLayout->addWidget(addButton);
            connect(addButton, &QPushButton::clicked, this,
                    &MainWindow::addSelectedExecutable);
        }
        const int columns =
            listMode ? 1 : qMax(1, (libraryScrollArea_->viewport()->width() + 14) / 250);
        libraryGrid_->addWidget(empty, 0, 0, 1, columns);
        rebuildingLibrary_ = false;
        return;
    }

    const int columns =
        listMode ? 1 : qMax(1, (libraryScrollArea_->viewport()->width() + 14) / 250);
    for (qsizetype index = 0; index < games.size(); ++index) {
        QWidget* gameCard = createGameCard(games.at(index), listMode);
        libraryGrid_->addWidget(gameCard, static_cast<int>(index / columns),
                                static_cast<int>(index % columns));
    }
    rebuildingLibrary_ = false;
}

void MainWindow::applySettings()
{
    const AppSettings& settings = settings_.settings();
    const bool dark = settings.theme == QStringLiteral("dark");
    const QString background = dark ? QStringLiteral("#0B0E14") : QStringLiteral("#F1F3F8");
    const QString panel = dark ? QStringLiteral("#121722") : QStringLiteral("#FFFFFF");
    const QString card = dark ? QStringLiteral("#191F2B") : QStringLiteral("#FFFFFF");
    const QString text = dark ? QStringLiteral("#F4F6FC") : QStringLiteral("#1B2130");
    const QString muted = dark ? QStringLiteral("#929BB0") : QStringLiteral("#687286");
    const QString border = dark ? QStringLiteral("#262D3B") : QStringLiteral("#DDE2EB");
    const QString accent = QColor(settings.accentColor).name(QColor::HexRgb);
    auto* application = static_cast<QApplication*>(QCoreApplication::instance());
    application->setStyleSheet(QStringLiteral(
        "QWidget#appShell { background: %1; color: %2; }"
        "QWidget#contentArea { background: %1; }"
        "QFrame#sidebar { background: %3; border-right: 1px solid %7; }"
        "QLabel { color: %2; }"
        "QLabel#brandMark { background: %8; color: white; border-radius: 10px; "
        "font-size: 20px; font-weight: 800; }"
        "QLabel#brandName { font-size: 15px; font-weight: 800; letter-spacing: 1px; }"
        "QLabel#brandSubtitle, QLabel#eyebrow, QLabel#sectionLabel { color: %5; "
        "font-size: 9px; font-weight: 700; letter-spacing: 1.1px; }"
        "QLabel#topBarTitle { color: %2; font-size: 17px; font-weight: 700; }"
        "QLabel#pageTitle { color: %2; font-size: 27px; font-weight: 750; }"
        "QLabel#sectionTitle { color: %2; font-size: 16px; font-weight: 700; }"
        "QLabel#settingLabel, QLabel#toolbarLabel { color: %2; font-weight: 600; }"
        "QLabel#mutedText, QLabel#localCardCopy, QLabel#heroSubtitle { color: %5; }"
        "QLabel#localCardTitle { color: %8; font-size: 10px; font-weight: 800; "
        "letter-spacing: 0.8px; }"
        "QLabel#statusPill { color: %8; background: %4; border: 1px solid %7; "
        "border-radius: 11px; padding: 7px 12px; font-size: 10px; font-weight: 700; }"
        "QLabel#summaryText { color: %2; font-size: 15px; font-weight: 600; }"
        "QLabel#recentList { color: %5; font-size: 12px; line-height: 1.5; }"
        "QLabel#heroEyebrow { color: #D8D0FF; font-size: 9px; font-weight: 800; "
        "letter-spacing: 1.1px; }"
        "QLabel#heroTitle { color: white; font-size: 26px; font-weight: 800; }"
        "QLabel#heroSubtitle { color: #D3D6E5; font-size: 12px; }"
        "QFrame#heroPanel { border: 1px solid #393154; border-radius: 16px; "
        "background: qlineargradient(x1:0, y1:0, x2:1, y2:1, "
        "stop:0 #282042, stop:0.55 #171D2B, stop:1 #11313B); }"
        "QFrame#surfaceCard { background: %4; border: 1px solid %7; "
        "border-radius: 12px; }"
        "QFrame#gameCard { background: %4; border: 1px solid %7; "
        "border-radius: 12px; }"
        "QFrame#gameCard:hover { border-color: %8; }"
        "QLineEdit, QComboBox { background: %3; color: %2; border: 1px solid %7; "
        "border-radius: 8px; padding: 8px 10px; min-height: 20px; }"
        "QComboBox QAbstractItemView { background: %3; color: %2; selection-background-color: %8; }"
        "QPushButton { color: %2; background: %4; border: 1px solid %7; "
        "border-radius: 8px; padding: 8px 12px; }"
        "QPushButton:hover { border-color: %8; }"
        "QPushButton#primaryButton { color: white; background: %8; border: 1px solid %8; "
        "font-weight: 700; }"
        "QPushButton#primaryButton:hover, QPushButton#heroButton:hover { "
        "background: %9; border-color: %9; }"
        "QPushButton#heroButton { color: white; background: %8; border: 0; "
        "border-radius: 9px; padding: 10px 17px; font-weight: 700; }"
        "QPushButton#secondaryButton, QPushButton#textButton { background: transparent; "
        "border-color: transparent; color: %8; font-weight: 650; }"
        "QPushButton#dangerButton { color: #F17B88; background: transparent; "
        "border-color: %7; }"
        "QPushButton#navButton { text-align: left; color: %5; background: transparent; "
        "border-color: transparent; padding: 8px 12px; font-weight: 600; }"
        "QPushButton#navButton[active=\"true\"] { color: %2; background: %4; "
        "border-color: %7; }"
        "QScrollArea { background: transparent; border: 0; }"
        "QCheckBox { color: %2; spacing: 8px; }"
        "QCheckBox::indicator { width: 17px; height: 17px; }"
        "QStatusBar { color: %5; }")
                            .arg(background, text, panel, card, muted, muted, border,
                                 accent, accent));
    for (QPushButton* nav : navigationButtons_) {
        nav->setMinimumHeight(settings.compactMode ? 34 : 42);
    }
    if (accentSwatch_ != nullptr) {
        accentSwatch_->setStyleSheet(
            QStringLiteral("background: %1; border-radius: 5px;").arg(accent));
    }
    if (libraryViewCombo_ != nullptr) {
        const QSignalBlocker blocker(libraryViewCombo_);
        libraryViewCombo_->setCurrentIndex(
            libraryViewCombo_->findData(settings.defaultLibraryView));
    }
    if (sortCombo_ != nullptr) {
        sortCombo_->setMinimumWidth(146);
    }
}

void MainWindow::openGameDialog(qint64 gameId)
{
    GameDialog dialog(this);
    if (gameId > 0) {
        QVector<Game> games = currentGames();
        const auto it = std::find_if(games.cbegin(), games.cend(),
                                     [gameId](const Game& game) {
            return game.id == gameId;
        });
        if (it == games.cend()) {
            showError(QStringLiteral("Game not found"),
                      QStringLiteral("This game is no longer in your library."));
            return;
        }
        GameDialog editDialog(*it, this);
        if (editDialog.exec() != QDialog::Accepted) {
            return;
        }
        QString error;
        if (!repository_.update(editDialog.game(), &error)) {
            showError(QStringLiteral("Could not save game"), error);
            return;
        }
        refreshAll();
        return;
    }

    if (dialog.exec() != QDialog::Accepted) {
        return;
    }
    QString error;
    if (!repository_.add(dialog.game(), nullptr, &error)) {
        showError(QStringLiteral("Could not add game"), error);
        return;
    }
    refreshAll();
}

void MainWindow::removeGame(const Game& game)
{
    if (launcher_.isRunning(game.id) || launcher_.isStarting(game.id)) {
        QMessageBox::information(
            this, QStringLiteral("Game is running"),
            QStringLiteral("Stop this game before removing it from your library."));
        return;
    }
    const auto answer = QMessageBox::question(
        this, QStringLiteral("Remove game from Nexus"),
        QStringLiteral("Remove “%1” from your library?\n\nThis only removes the Nexus entry. "
                       "Game files, saves, and settings will not be deleted.")
            .arg(game.title));
    if (answer != QMessageBox::Yes) {
        return;
    }
    QString error;
    if (!repository_.remove(game.id, &error)) {
        showError(QStringLiteral("Could not remove game"), error);
        return;
    }
    refreshAll();
}

QWidget* MainWindow::createGameCard(const Game& game, bool listMode)
{
    auto* card = new QFrame(libraryContents_);
    card->setObjectName(QStringLiteral("gameCard"));
    card->setFrameShape(QFrame::NoFrame);
    if (listMode) {
        card->setMinimumHeight(116);
        card->setMaximumHeight(140);
    } else {
        card->setMinimumWidth(220);
        card->setMaximumWidth(340);
    }

    auto* root = new QHBoxLayout(card);
    root->setContentsMargins(10, 10, 10, 10);
    root->setSpacing(12);
    auto* art = new QLabel(card);
    art->setObjectName(QStringLiteral("coverArt"));
    art->setAlignment(Qt::AlignCenter);
    art->setMinimumSize(listMode ? QSize(94, 92) : QSize(0, 132));
    if (!listMode) {
        art->setMaximumHeight(132);
    }
    bool hasCover = false;
    if (!game.coverPath.isEmpty() && QFileInfo(game.coverPath).isFile()) {
        const QPixmap cover(game.coverPath);
        if (!cover.isNull()) {
            art->setPixmap(cover.scaled(listMode ? QSize(94, 92) : QSize(300, 132),
                                        Qt::KeepAspectRatioByExpanding,
                                        Qt::SmoothTransformation));
            art->setStyleSheet(QStringLiteral("border-radius: 8px; background: #10151F;"));
            hasCover = true;
        }
    }
    if (!hasCover) {
        art->setText(game.title.left(1).toUpper());
        art->setStyleSheet(QStringLiteral(
            "background: qlineargradient(x1:0,y1:0,x2:1,y2:1,stop:0 #292143,"
            "stop:0.56 #192333,stop:1 #17343A); border-radius: 8px; "
            "color: #E5E0FF; font-size: 36px; font-weight: 800;"));
    }

    auto* details = new QVBoxLayout();
    details->setSpacing(5);
    auto* title = label(game.title, QStringLiteral("gameTitle"), card);
    title->setWordWrap(true);
    details->addWidget(title);
    auto* metadata = label(
        QStringLiteral("%1  ·  %2")
            .arg(game.launcher, formattedPlaytime(game.totalPlaySeconds)),
        QStringLiteral("gameMetadata"), card);
    details->addWidget(metadata);
    QStringList organization;
    for (const QString& value :
         {game.genre, game.category, game.collection, game.franchise}) {
        if (!value.trimmed().isEmpty() && !organization.contains(value.trimmed())) {
            organization.append(value.trimmed());
        }
    }
    if (!organization.isEmpty()) {
        auto* organizationLabel = label(organization.join(QStringLiteral("  ·  ")),
                                        QStringLiteral("gameTags"), card);
        organizationLabel->setWordWrap(true);
        details->addWidget(organizationLabel);
    }
    if (!game.tags.isEmpty()) {
        auto* tags = label(game.tags.mid(0, 3).join(QStringLiteral("  ·  ")),
                           QStringLiteral("gameTags"), card);
        tags->setWordWrap(true);
        details->addWidget(tags);
    }
    if (!game.executableExists()) {
        auto* missing = label(QStringLiteral("Executable missing · edit to repair"),
                              QStringLiteral("missingPath"), card);
        details->addWidget(missing);
    } else if (game.lastPlayed.isValid()) {
        details->addWidget(label(QStringLiteral("Last played %1")
                                     .arg(formattedLastPlayed(game.lastPlayed)),
                                 QStringLiteral("gameMetadata"), card));
    }
    details->addStretch();

    auto* actions = new QVBoxLayout();
    actions->setSpacing(6);
    auto* favorite = button(game.favorite ? QStringLiteral("★") : QStringLiteral("☆"),
                            QStringLiteral("favoriteButton"), card);
    favorite->setToolTip(game.favorite ? QStringLiteral("Remove favorite")
                                      : QStringLiteral("Add favorite"));
    favorite->setFixedWidth(40);
    connect(favorite, &QPushButton::clicked, this, [this, game]() {
        QString error;
        if (!repository_.setFavorite(game.id, !game.favorite, &error)) {
            showError(QStringLiteral("Could not update favorite"), error);
            return;
        }
        refreshAll();
    });

    auto* play = button(QStringLiteral("Play"), QStringLiteral("primaryButton"), card);
    play->setEnabled(game.executableExists() && !launcher_.isStarting(game.id));
    if (launcher_.isStarting(game.id)) {
        play->setText(QStringLiteral("Starting…"));
    } else if (launcher_.isRunning(game.id)) {
        play->setText(QStringLiteral("Focus"));
        connect(play, &QPushButton::clicked, this, [this, game]() {
            QString error;
            if (!launcher_.focus(game.id, &error)) {
                showError(QStringLiteral("Could not focus game"), error);
            }
        });
        auto* stop = button(QStringLiteral("Stop"), QStringLiteral("dangerButton"), card);
        connect(stop, &QPushButton::clicked, this, [this, game]() {
            const auto answer = QMessageBox::question(
                this, QStringLiteral("Stop game"),
                QStringLiteral("Close the running process for “%1”?")
                    .arg(game.title));
            if (answer != QMessageBox::Yes) {
                return;
            }
            QString error;
            if (!launcher_.stop(game.id, &error)) {
                showError(QStringLiteral("Could not stop game"), error);
            }
        });
        actions->addWidget(stop);
    } else {
        connect(play, &QPushButton::clicked, this, [this, game]() {
            QString error;
            if (!launcher_.launch(game, &error)) {
                showError(QStringLiteral("Could not start the game"), error);
            } else {
                statusLabel_->setText(QStringLiteral("Starting game…"));
            }
        });
    }

    auto* edit = button(QStringLiteral("Edit"), QStringLiteral("secondaryButton"), card);
    connect(edit, &QPushButton::clicked, this,
            [this, game]() { openGameDialog(game.id); });
    auto* remove = button(QStringLiteral("Remove"), QStringLiteral("dangerButton"), card);
    connect(remove, &QPushButton::clicked, this,
            [this, game]() { removeGame(game); });

    const bool customOrder =
        sortCombo_ != nullptr &&
        sortCombo_->currentData().toInt() == static_cast<int>(GameSort::Custom);
    auto* moveEarlier = button(QStringLiteral("↑"), QStringLiteral("secondaryButton"), card);
    moveEarlier->setToolTip(QStringLiteral("Move earlier in custom order"));
    moveEarlier->setVisible(customOrder);
    connect(moveEarlier, &QPushButton::clicked, this, [this, game]() {
        QString error;
        if (!repository_.moveGame(game.id, -1, &error)) {
            showError(QStringLiteral("Could not reorder games"), error);
            return;
        }
        refreshLibrary();
    });
    auto* moveLater = button(QStringLiteral("↓"), QStringLiteral("secondaryButton"), card);
    moveLater->setToolTip(QStringLiteral("Move later in custom order"));
    moveLater->setVisible(customOrder);
    connect(moveLater, &QPushButton::clicked, this, [this, game]() {
        QString error;
        if (!repository_.moveGame(game.id, 1, &error)) {
            showError(QStringLiteral("Could not reorder games"), error);
            return;
        }
        refreshLibrary();
    });

    if (listMode) {
        root->addWidget(art);
        root->addLayout(details, 1);
        auto* right = new QVBoxLayout();
        right->addWidget(favorite, 0, Qt::AlignRight);
        right->addStretch();
        right->addWidget(play);
        root->addLayout(right);
        actions->addWidget(edit);
        actions->addWidget(remove);
        if (customOrder) {
            actions->addWidget(moveEarlier);
            actions->addWidget(moveLater);
        }
        root->addLayout(actions);
    } else {
        auto* body = new QVBoxLayout();
        body->setSpacing(10);
        body->addWidget(art);
        auto* content = new QHBoxLayout();
        content->addLayout(details, 1);
        content->addWidget(favorite, 0, Qt::AlignTop);
        body->addLayout(content);
        auto* footer = new QHBoxLayout();
        footer->addWidget(play, 1);
        footer->addWidget(edit);
        footer->addWidget(remove);
        if (customOrder) {
            footer->addWidget(moveEarlier);
            footer->addWidget(moveLater);
        }
        body->addLayout(footer);
        root->addLayout(body, 1);
    }
    return card;
}

void MainWindow::addSelectedExecutable()
{
    openGameDialog();
}

void MainWindow::showError(const QString& title, const QString& details)
{
    QMessageBox::critical(this, title, details);
}

QVector<Game> MainWindow::currentGames(QString* error) const
{
    QVector<Game> games;
    GameSort sort = GameSort::Name;
    if (sortCombo_ != nullptr) {
        sort = static_cast<GameSort>(sortCombo_->currentData().toInt());
    }
    if (!repository_.list(games, error, sort)) {
        return {};
    }
    return games;
}

void MainWindow::resizeEvent(QResizeEvent* event)
{
    QMainWindow::resizeEvent(event);
    if (currentPage_ == Page::Library) {
        QTimer::singleShot(80, this, [this]() { refreshLibrary(); });
    }
}

} // namespace nexus
