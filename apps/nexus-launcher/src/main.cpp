#include "nexus/app/app_settings.h"
#include "nexus/data/database.h"
#include "nexus/data/game_repository.h"
#include "nexus/launcher/game_launcher.h"
#include "nexus/ui/main_window.h"

#include <QApplication>
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFont>
#include <QIcon>
#include <QLinearGradient>
#include <QMessageBox>
#include <QMutex>
#include <QMutexLocker>
#include <QPainter>
#include <QPixmap>
#include <QSplashScreen>
#include <QStandardPaths>
#include <QTextStream>

#include <cstdlib>

namespace {

QFile* applicationLog = nullptr;
QMutex logMutex;

void writeLog(QtMsgType type, const QMessageLogContext&, const QString& message)
{
    QMutexLocker lock(&logMutex);
    if (applicationLog != nullptr && applicationLog->isOpen()) {
        QTextStream stream(applicationLog);
        stream << QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)
               << QStringLiteral(" [") << static_cast<int>(type)
               << QStringLiteral("] ") << message << Qt::endl;
        applicationLog->flush();
    }
    if (type == QtFatalMsg) {
        std::abort();
    }
}

void configureLogging()
{
    const QString directory =
        QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    QDir().mkpath(QDir(directory).filePath(QStringLiteral("logs")));
    applicationLog = new QFile(
        QDir(directory).filePath(QStringLiteral("logs/nexus.log")));
    if (applicationLog->open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        qInstallMessageHandler(writeLog);
    }
}

QPixmap createBrandArtwork(const QSize& size, bool splash)
{
    QPixmap image(size);
    image.fill(QColor(QStringLiteral("#0B0E14")));
    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing);

    const QRectF markRect(size.width() * 0.08, size.height() * 0.13,
                          size.height() * 0.74, size.height() * 0.74);
    QLinearGradient gradient(markRect.topLeft(), markRect.bottomRight());
    gradient.setColorAt(0.0, QColor(QStringLiteral("#7C5CFC")));
    gradient.setColorAt(1.0, QColor(QStringLiteral("#00AFCB")));
    painter.setBrush(gradient);
    painter.setPen(Qt::NoPen);
    painter.drawRoundedRect(markRect, size.height() * 0.18, size.height() * 0.18);
    painter.setPen(QPen(Qt::white, size.height() * 0.075, Qt::SolidLine,
                        Qt::RoundCap, Qt::RoundJoin));
    const qreal inset = markRect.width() * 0.28;
    painter.drawLine(QPointF(markRect.left() + inset, markRect.bottom() - inset),
                     QPointF(markRect.left() + inset, markRect.top() + inset));
    painter.drawLine(QPointF(markRect.left() + inset, markRect.top() + inset),
                     QPointF(markRect.right() - inset, markRect.bottom() - inset));
    painter.drawLine(QPointF(markRect.right() - inset, markRect.bottom() - inset),
                     QPointF(markRect.right() - inset, markRect.top() + inset));

    if (splash) {
        const qreal textLeft = markRect.right() + size.width() * 0.06;
        painter.setPen(QColor(QStringLiteral("#F4F6FC")));
        QFont titleFont(QStringLiteral("Segoe UI"));
        titleFont.setPixelSize(qMax(18, size.height() / 8));
        titleFont.setWeight(QFont::Bold);
        painter.setFont(titleFont);
        painter.drawText(QRectF(textLeft, size.height() * 0.31,
                                size.width() - textLeft - 12, size.height() * 0.22),
                         Qt::AlignLeft | Qt::AlignVCenter,
                         QStringLiteral("NEXUS"));
        painter.setPen(QColor(QStringLiteral("#929BB0")));
        QFont subtitleFont(QStringLiteral("Segoe UI"));
        subtitleFont.setPixelSize(qMax(10, size.height() / 24));
        painter.setFont(subtitleFont);
        painter.drawText(QRectF(textLeft, size.height() * 0.54,
                                size.width() - textLeft - 12, size.height() * 0.18),
                         Qt::AlignLeft | Qt::AlignVCenter,
                         QStringLiteral("YOUR GAMES. ONE PLACE."));
    }
    return image;
}

} // namespace

int main(int argc, char* argv[])
{
    QApplication application(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("Nexus"));
    QCoreApplication::setApplicationName(QStringLiteral("NexusGameLauncher"));
    QCoreApplication::setApplicationVersion(QStringLiteral("0.1.0"));
    QApplication::setApplicationDisplayName(QStringLiteral("Nexus Game Launcher"));
    application.setStyle(QStringLiteral("Fusion"));
    application.setWindowIcon(QIcon(createBrandArtwork(QSize(128, 128), false)));

    configureLogging();

    nexus::SettingsStore settings;
    QString settingsWarning;
    if (!settings.load(&settingsWarning)) {
        QMessageBox::warning(nullptr, QStringLiteral("Settings unavailable"),
                             settingsWarning);
    } else if (!settingsWarning.isEmpty()) {
        QMessageBox::warning(nullptr, QStringLiteral("Settings restored"),
                             settingsWarning);
    }

    QSplashScreen splash(createBrandArtwork(QSize(520, 210), true));
    splash.show();
    application.processEvents();

    nexus::Database database;
    QString databaseError;
    if (!database.open(&databaseError)) {
        splash.close();
        QMessageBox::critical(nullptr, QStringLiteral("Nexus could not start"),
                              databaseError);
        return 1;
    }

    nexus::GameRepository repository(database);
    QString recoveryError;
    if (!repository.recoverInterruptedSessions(&recoveryError)) {
        qWarning().noquote() << QStringLiteral("Session recovery failed: %1")
                                    .arg(recoveryError);
    }

    nexus::GameLauncher launcher(repository);
    nexus::MainWindow window(settings, repository, launcher);
    window.show();
    splash.finish(&window);
    return application.exec();
}
