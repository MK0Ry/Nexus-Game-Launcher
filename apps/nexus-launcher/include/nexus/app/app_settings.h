#pragma once

#include <QString>

namespace nexus {

struct AppSettings {
    QString theme = QStringLiteral("dark");
    QString accentColor = QStringLiteral("#7C5CFC");
    bool compactMode = false;
    QString defaultLibraryView = QStringLiteral("grid");
};

class SettingsStore {
public:
    explicit SettingsStore(QString filePath = {});

    [[nodiscard]] const AppSettings& settings() const;
    [[nodiscard]] QString filePath() const;
    bool load(QString* warning = nullptr);
    bool save(const AppSettings& settings, QString* error = nullptr);
    bool reset(QString* error = nullptr);

private:
    QString filePath_;
    AppSettings settings_;
};

} // namespace nexus
