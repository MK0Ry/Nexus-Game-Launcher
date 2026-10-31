#pragma once

#include "nexus/model/game.h"

#include <QDialog>

class QLineEdit;
class QTextEdit;

namespace nexus {

class GameDialog : public QDialog {
    Q_OBJECT

public:
    explicit GameDialog(QWidget* parent = nullptr);
    GameDialog(const Game& game, QWidget* parent = nullptr);

    [[nodiscard]] Game game() const;

private:
    void buildUi(const Game* game);
    void browseExecutable();
    void browseCover();
    void accept() override;

    Game initial_;
    QLineEdit* titleEdit_ = nullptr;
    QLineEdit* executableEdit_ = nullptr;
    QLineEdit* workingDirectoryEdit_ = nullptr;
    QLineEdit* argumentsEdit_ = nullptr;
    QLineEdit* launcherEdit_ = nullptr;
    QLineEdit* genreEdit_ = nullptr;
    QLineEdit* categoryEdit_ = nullptr;
    QLineEdit* collectionEdit_ = nullptr;
    QLineEdit* franchiseEdit_ = nullptr;
    QLineEdit* tagsEdit_ = nullptr;
    QLineEdit* coverEdit_ = nullptr;
    QTextEdit* descriptionEdit_ = nullptr;
};

} // namespace nexus
