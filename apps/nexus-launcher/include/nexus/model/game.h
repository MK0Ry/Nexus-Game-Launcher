#pragma once

#include <QDateTime>
#include <QString>
#include <QStringList>

namespace nexus {

struct Game {
    qint64 id = 0;
    QString title;
    QString description;
    QString executablePath;
    QString workingDirectory;
    QString launchArguments;
    QString coverPath;
    QString launcher = QStringLiteral("Manual");
    QString genre;
    QString category;
    QString collection;
    QString franchise;
    QStringList tags;
    bool favorite = false;
    qint64 launchCount = 0;
    qint64 totalPlaySeconds = 0;
    qint64 customOrder = 0;
    QDateTime lastPlayed;
    QDateTime dateAdded;

    [[nodiscard]] bool executableExists() const;
};

} // namespace nexus
