#include "nexus/app/app_settings.h"

#include <QColor>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QStandardPaths>

namespace nexus {
namespace {

AppSettings defaultSettings()
{
    return {};
}

QString defaultSettingsPath()
{
    const QString directory =
        QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    return QDir(directory).filePath(QStringLiteral("nexus-settings.json"));
}

} // namespace

SettingsStore::SettingsStore(QString filePath)
    : filePath_(filePath.isEmpty() ? defaultSettingsPath() : std::move(filePath))
{
}

const AppSettings& SettingsStore::settings() const
{
    return settings_;
}

QString SettingsStore::filePath() const
{
    return filePath_;
}

bool SettingsStore::load(QString* warning)
{
    settings_ = defaultSettings();
    if (warning != nullptr) {
        warning->clear();
    }

    QFile file(filePath_);
    if (!file.exists()) {
        return true;
    }
    if (!file.open(QIODevice::ReadOnly)) {
        if (warning != nullptr) {
            *warning = QStringLiteral("Could not read settings: %1").arg(file.errorString());
        }
        return false;
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
    file.close();
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        const QString backupPath =
            filePath_ + QStringLiteral(".corrupt-") +
            QString::number(QDateTime::currentMSecsSinceEpoch());
        const bool preserved = QFile::rename(filePath_, backupPath);
        if (warning != nullptr) {
            *warning = preserved
                           ? QStringLiteral(
                                 "Settings were malformed and could not be loaded. "
                                 "Defaults are active; the original file was kept at %1.")
                                 .arg(backupPath)
                           : QStringLiteral(
                                 "Settings were malformed and could not be loaded. "
                                 "Defaults are active; the original file could not be renamed "
                                 "and remains at %1.")
                                 .arg(filePath_);
        }
        return true;
    }

    const QJsonObject object = document.object();
    const QString theme = object.value(QStringLiteral("theme")).toString();
    if (theme == QStringLiteral("dark") || theme == QStringLiteral("light")) {
        settings_.theme = theme;
    }

    const QString accent = object.value(QStringLiteral("accentColor")).toString();
    if (QColor(accent).isValid()) {
        settings_.accentColor = accent;
    }

    settings_.compactMode = object.value(QStringLiteral("compactMode")).toBool(false);
    const QString view = object.value(QStringLiteral("defaultLibraryView")).toString();
    if (view == QStringLiteral("grid") || view == QStringLiteral("list")) {
        settings_.defaultLibraryView = view;
    }
    return true;
}

bool SettingsStore::save(const AppSettings& settings, QString* error)
{
    if (error != nullptr) {
        error->clear();
    }

    const QFileInfo info(filePath_);
    if (!QDir().mkpath(info.absolutePath())) {
        if (error != nullptr) {
            *error = QStringLiteral("Could not create the settings directory.");
        }
        return false;
    }

    const QColor accent(settings.accentColor);
    if ((settings.theme != QStringLiteral("dark") &&
         settings.theme != QStringLiteral("light")) ||
        !accent.isValid() ||
        (settings.defaultLibraryView != QStringLiteral("grid") &&
         settings.defaultLibraryView != QStringLiteral("list"))) {
        if (error != nullptr) {
            *error = QStringLiteral("One or more settings values are invalid.");
        }
        return false;
    }

    QJsonObject object;
    object.insert(QStringLiteral("schemaVersion"), 1);
    object.insert(QStringLiteral("theme"), settings.theme);
    object.insert(QStringLiteral("accentColor"), accent.name(QColor::HexRgb));
    object.insert(QStringLiteral("compactMode"), settings.compactMode);
    object.insert(QStringLiteral("defaultLibraryView"), settings.defaultLibraryView);

    QSaveFile file(filePath_);
    if (!file.open(QIODevice::WriteOnly)) {
        if (error != nullptr) {
            *error = QStringLiteral("Could not write settings: %1").arg(file.errorString());
        }
        return false;
    }
    const QByteArray serialized = QJsonDocument(object).toJson(QJsonDocument::Indented);
    if (file.write(serialized) != serialized.size()) {
        if (error != nullptr) {
            *error = QStringLiteral("Could not write settings: %1").arg(file.errorString());
        }
        file.cancelWriting();
        return false;
    }
    if (!file.commit()) {
        if (error != nullptr) {
            *error = QStringLiteral("Could not safely save settings: %1").arg(file.errorString());
        }
        return false;
    }

    settings_ = settings;
    settings_.accentColor = accent.name(QColor::HexRgb);
    return true;
}

bool SettingsStore::reset(QString* error)
{
    return save(defaultSettings(), error);
}

} // namespace nexus
