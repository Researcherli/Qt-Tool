#include "services/AppPaths.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QSettings>

namespace est
{

    QString AppPaths::dataRoot()
    {
        QString appDirPath;
        if (QCoreApplication::instance() != nullptr)
        {
            appDirPath = QCoreApplication::applicationDirPath();
        }
        if (appDirPath.isEmpty())
        {
            appDirPath = QDir::currentPath();
        }

        return ensureDirectory(QDir(appDirPath).filePath(QStringLiteral("EmbeddedSoftwareToolsData")));
    }

    QString AppPaths::configDir()
    {
        return ensureDirectory(QDir(dataRoot()).filePath(QStringLiteral("config")));
    }

    QString AppPaths::logsDir()
    {
        return ensureDirectory(QDir(dataRoot()).filePath(QStringLiteral("logs")));
    }

    QString AppPaths::exportsDir()
    {
        return ensureDirectory(QDir(dataRoot()).filePath(QStringLiteral("exports")));
    }

    QString AppPaths::recordingsDir()
    {
        return ensureDirectory(QDir(dataRoot()).filePath(QStringLiteral("recordings")));
    }

    QString AppPaths::scriptsDir()
    {
        return ensureDirectory(QDir(dataRoot()).filePath(QStringLiteral("scripts")));
    }

    QString AppPaths::configFilePath(const QString &fileName)
    {
        return QDir(configDir()).filePath(fileName);
    }

    QString AppPaths::exportFilePath(const QString &fileName)
    {
        return QDir(exportsDir()).filePath(fileName);
    }

    QString AppPaths::logFilePath(const QString &fileName)
    {
        return QDir(logsDir()).filePath(fileName);
    }

    QString AppPaths::recordingFilePath(const QString &fileName)
    {
        return QDir(recordingsDir()).filePath(fileName);
    }

    QString AppPaths::scriptFilePath(const QString &fileName)
    {
        return QDir(scriptsDir()).filePath(fileName);
    }

    void AppPaths::configureQSettings()
    {
        QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, configDir());
    }

    bool AppPaths::isDriveCPath(const QString &path)
    {
        const QString absolutePath = QFileInfo(path).absoluteFilePath();
        return absolutePath.size() >= 2
               && absolutePath.at(1) == QLatin1Char(':')
               && absolutePath.at(0).toUpper() == QLatin1Char('C');
    }

    QString AppPaths::ensureDirectory(const QString &path)
    {
        QDir dir(path);
        dir.mkpath(QStringLiteral("."));
        return dir.absolutePath();
    }

} // namespace est
