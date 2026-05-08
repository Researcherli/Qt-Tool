#include "AppShell.h"

#include <QApplication>
#include <QFile>
#include <QIcon>
#include <QStyleFactory>
#include <QDebug>
#include <QStandardPaths>

void logToFile(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
    QByteArray localMsg = msg.toLocal8Bit();
    QFile file("C:\\Users\\19893\\AppData\\Local\\Temp\\opencode\\crash_log.txt");
    if (!file.open(QIODevice::WriteOnly | QIODevice::Append))
    {
        return;
    }
    file.write(localMsg);
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
    app.setApplicationName(QStringLiteral("嵌入式工具"));
    app.setApplicationDisplayName(QStringLiteral("嵌入式工具"));
    app.setApplicationVersion(QStringLiteral("2.0.0"));
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
