#include "nexus/ui/game_dialog.h"

#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QTextEdit>
#include <QVBoxLayout>

namespace nexus {

GameDialog::GameDialog(QWidget* parent)
    : QDialog(parent)
{
    buildUi(nullptr);
}

GameDialog::GameDialog(const Game& game, QWidget* parent)
    : QDialog(parent),
      initial_(game)
{
    buildUi(&game);
}

Game GameDialog::game() const
{
    Game result = initial_;
    result.title = titleEdit_->text().trimmed();
    result.executablePath = executableEdit_->text().trimmed();
    result.workingDirectory = workingDirectoryEdit_->text().trimmed();
    result.launchArguments = argumentsEdit_->text().trimmed();
    result.launcher = launcherEdit_->text().trimmed();
    result.genre = genreEdit_->text().trimmed();
    result.category = categoryEdit_->text().trimmed();
    result.collection = collectionEdit_->text().trimmed();
    result.franchise = franchiseEdit_->text().trimmed();
    result.coverPath = coverEdit_->text().trimmed();
    result.description = descriptionEdit_->toPlainText().trimmed();
    result.tags.clear();
    for (const QString& tag : tagsEdit_->text().split(QLatin1Char(','), Qt::SkipEmptyParts)) {
        const QString normalized = tag.trimmed();
        if (!normalized.isEmpty() && !result.tags.contains(normalized, Qt::CaseInsensitive)) {
            result.tags.append(normalized);
        }
    }
    return result;
}

void GameDialog::buildUi(const Game* game)
{
    setWindowTitle(game == nullptr ? QStringLiteral("Add a game")
                                  : QStringLiteral("Edit game"));
    setModal(true);
    setMinimumWidth(560);
    setStyleSheet(QStringLiteral(
        "QDialog { background: #121722; color: #F4F6FC; }"
        "QLabel { color: #C7CDDA; }"
        "QLineEdit, QTextEdit { background: #0B0E14; color: #F4F6FC; "
        "border: 1px solid #2A3140; border-radius: 7px; padding: 8px; }"
        "QPushButton { background: #242B38; color: #F4F6FC; border: 0; "
        "border-radius: 7px; padding: 8px 14px; }"
        "QPushButton:hover { background: #30394A; }"));

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(24, 22, 24, 20);
    root->setSpacing(16);
    auto* heading = new QLabel(game == nullptr ? QStringLiteral("Add to your collection")
                                              : QStringLiteral("Game details"), this);
    heading->setStyleSheet(QStringLiteral("font-size: 20px; font-weight: 700; color: #F4F6FC;"));
    root->addWidget(heading);

    auto* form = new QFormLayout();
    form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    form->setHorizontalSpacing(18);
    form->setVerticalSpacing(12);

    titleEdit_ = new QLineEdit(this);
    titleEdit_->setPlaceholderText(QStringLiteral("Game title"));
    executableEdit_ = new QLineEdit(this);
    executableEdit_->setPlaceholderText(QStringLiteral("Select the game executable"));
    auto* executableRow = new QWidget(this);
    auto* executableLayout = new QHBoxLayout(executableRow);
    executableLayout->setContentsMargins(0, 0, 0, 0);
    executableLayout->setSpacing(8);
    executableLayout->addWidget(executableEdit_, 1);
    auto* executableBrowse = new QPushButton(QStringLiteral("Browse"), executableRow);
    connect(executableBrowse, &QPushButton::clicked, this, &GameDialog::browseExecutable);
    executableLayout->addWidget(executableBrowse);

    workingDirectoryEdit_ = new QLineEdit(this);
    workingDirectoryEdit_->setPlaceholderText(QStringLiteral("Defaults to the executable's folder"));
    argumentsEdit_ = new QLineEdit(this);
    argumentsEdit_->setPlaceholderText(QStringLiteral("Optional command-line arguments"));
    launcherEdit_ = new QLineEdit(this);
    launcherEdit_->setPlaceholderText(QStringLiteral("Manual"));
    genreEdit_ = new QLineEdit(this);
    genreEdit_->setPlaceholderText(QStringLiteral("Optional genre"));
    categoryEdit_ = new QLineEdit(this);
    categoryEdit_->setPlaceholderText(QStringLiteral("Optional category"));
    collectionEdit_ = new QLineEdit(this);
    collectionEdit_->setPlaceholderText(QStringLiteral("Optional collection"));
    franchiseEdit_ = new QLineEdit(this);
    franchiseEdit_->setPlaceholderText(QStringLiteral("Optional franchise"));
    tagsEdit_ = new QLineEdit(this);
    tagsEdit_->setPlaceholderText(QStringLiteral("Separate tags with commas"));

    coverEdit_ = new QLineEdit(this);
    coverEdit_->setPlaceholderText(QStringLiteral("Optional local image path"));
    auto* coverRow = new QWidget(this);
    auto* coverLayout = new QHBoxLayout(coverRow);
    coverLayout->setContentsMargins(0, 0, 0, 0);
    coverLayout->setSpacing(8);
    coverLayout->addWidget(coverEdit_, 1);
    auto* coverBrowse = new QPushButton(QStringLiteral("Choose image"), coverRow);
    connect(coverBrowse, &QPushButton::clicked, this, &GameDialog::browseCover);
    coverLayout->addWidget(coverBrowse);

    descriptionEdit_ = new QTextEdit(this);
    descriptionEdit_->setPlaceholderText(QStringLiteral("A short note about this game"));
    descriptionEdit_->setFixedHeight(82);

    form->addRow(QStringLiteral("Title"), titleEdit_);
    form->addRow(QStringLiteral("Executable"), executableRow);
    form->addRow(QStringLiteral("Working folder"), workingDirectoryEdit_);
    form->addRow(QStringLiteral("Launch arguments"), argumentsEdit_);
    form->addRow(QStringLiteral("Launcher label"), launcherEdit_);
    form->addRow(QStringLiteral("Genre"), genreEdit_);
    form->addRow(QStringLiteral("Category"), categoryEdit_);
    form->addRow(QStringLiteral("Collection"), collectionEdit_);
    form->addRow(QStringLiteral("Franchise"), franchiseEdit_);
    form->addRow(QStringLiteral("Tags"), tagsEdit_);
    form->addRow(QStringLiteral("Cover art"), coverRow);
    form->addRow(QStringLiteral("Description"), descriptionEdit_);
    root->addLayout(form);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel, this);
    buttons->button(QDialogButtonBox::Save)->setText(QStringLiteral("Save game"));
    buttons->button(QDialogButtonBox::Save)->setStyleSheet(
        QStringLiteral("background: #7C5CFC; font-weight: 700; padding: 9px 18px;"));
    connect(buttons, &QDialogButtonBox::accepted, this, &GameDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &GameDialog::reject);
    root->addWidget(buttons);

    if (game != nullptr) {
        titleEdit_->setText(game->title);
        executableEdit_->setText(game->executablePath);
        workingDirectoryEdit_->setText(game->workingDirectory);
        argumentsEdit_->setText(game->launchArguments);
        launcherEdit_->setText(game->launcher);
        genreEdit_->setText(game->genre);
        categoryEdit_->setText(game->category);
        collectionEdit_->setText(game->collection);
        franchiseEdit_->setText(game->franchise);
        tagsEdit_->setText(game->tags.join(QStringLiteral(", ")));
        coverEdit_->setText(game->coverPath);
        descriptionEdit_->setPlainText(game->description);
    }
}

void GameDialog::browseExecutable()
{
    const QString path = QFileDialog::getOpenFileName(
        this, QStringLiteral("Select game executable"), executableEdit_->text(),
        QStringLiteral("Windows applications (*.exe);;All files (*.*)"));
    if (path.isEmpty()) {
        return;
    }
    executableEdit_->setText(path);
    if (titleEdit_->text().trimmed().isEmpty()) {
        titleEdit_->setText(QFileInfo(path).completeBaseName());
    }
    if (workingDirectoryEdit_->text().trimmed().isEmpty()) {
        workingDirectoryEdit_->setText(QFileInfo(path).absolutePath());
    }
}

void GameDialog::browseCover()
{
    const QString path = QFileDialog::getOpenFileName(
        this, QStringLiteral("Choose cover image"), coverEdit_->text(),
        QStringLiteral("Images (*.png *.jpg *.jpeg *.webp *.bmp)"));
    if (!path.isEmpty()) {
        coverEdit_->setText(path);
    }
}

void GameDialog::accept()
{
    const Game value = game();
    if (value.title.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("Title required"),
                             QStringLiteral("Enter a title for this game."));
        titleEdit_->setFocus();
        return;
    }
    const QFileInfo executable(value.executablePath);
    if (!executable.exists() || !executable.isFile()) {
        QMessageBox::warning(this, QStringLiteral("Executable required"),
                             QStringLiteral("Choose an existing game executable."));
        executableEdit_->setFocus();
        return;
    }
    if (!value.workingDirectory.isEmpty() &&
        !QFileInfo(value.workingDirectory).isDir()) {
        QMessageBox::warning(this, QStringLiteral("Folder not found"),
                             QStringLiteral("The selected working folder does not exist."));
        workingDirectoryEdit_->setFocus();
        return;
    }
    if (!value.coverPath.isEmpty() && !QFileInfo(value.coverPath).isFile()) {
        QMessageBox::warning(this, QStringLiteral("Cover image not found"),
                             QStringLiteral("Choose an existing image or clear the cover field."));
        coverEdit_->setFocus();
        return;
    }
    QDialog::accept();
}

} // namespace nexus
