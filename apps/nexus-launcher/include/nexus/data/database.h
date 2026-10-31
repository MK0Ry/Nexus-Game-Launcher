#pragma once

#include <QSqlDatabase>
#include <QString>

namespace nexus {

class Database {
public:
    explicit Database(QString databasePath = {});
    ~Database();

    Database(const Database&) = delete;
    Database& operator=(const Database&) = delete;

    bool open(QString* error = nullptr);
    [[nodiscard]] QSqlDatabase connection() const;
    [[nodiscard]] QString filePath() const;

private:
    bool migrate(QString* error);

    QString databasePath_;
    QString connectionName_;
    QSqlDatabase connection_;
};

} // namespace nexus
