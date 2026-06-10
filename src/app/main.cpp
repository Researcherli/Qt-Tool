#include "AppShell.h"

#include "services/AppPaths.h"

#include <QApplication>
#include <QFile>
#include <QIcon>
#include <QStyleFactory>
#include <QDebug>

void logToFile(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
    Q_UNUSED(type)
    Q_UNUSED(context)

    QFile file(est::AppPaths::logFilePath(QStringLiteral("crash_log.txt")));
    if (!file.open(QIODevice::WriteOnly | QIODevice::Append))
    {
        return;
    }
    file.write(msg.toLocal8Bit());
    file.write("\n");
    file.close();
}

int main(int argc, char *argv[])
{
    qInstallMessageHandler(logToFile);
    qDebug() << "App starting";
    QApplication app(argc, argv);
    qDebug() << "QApplication created";


    // 应用信息
    app.setOrganizationName("EST");
    app.setOrganizationDomain("embedded-software-tools.local");
    app.setApplicationName(QStringLiteral("EST Studio"));
    app.setApplicationDisplayName(QStringLiteral("EST Studio"));
    app.setApplicationVersion(QStringLiteral("2.0.0"));
    est::AppPaths::configureQSettings();
    app.setWindowIcon(QIcon(QStringLiteral(":/icons/app_icon_256.png")));
    app.setStyle(QStyleFactory::create(QStringLiteral("Fusion")));

    QFile themeFile(QStringLiteral(":/themes/wood_classic.qss"));
    if (themeFile.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        app.setStyleSheet(QString::fromUtf8(themeFile.readAll()));
    }

    est::AppShell shell;
    qDebug() << "AppShell constructed";
    if (!shell.initialize())
    {
        qDebug() << "AppShell initialize failed";
        return -1;
    }
    qDebug() << "AppShell initialized";

    shell.show();
    qDebug() << "AppShell shown";

    return app.exec();
}
