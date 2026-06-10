#ifndef EST_APP_PATHS_H
#define EST_APP_PATHS_H

#include <QString>

namespace est
{

    class AppPaths
    {
    public:
        static QString dataRoot();
        static QString configDir();
        static QString logsDir();
        static QString exportsDir();
        static QString recordingsDir();
        static QString scriptsDir();

        static QString configFilePath(const QString &fileName);
        static QString exportFilePath(const QString &fileName);
        static QString logFilePath(const QString &fileName);
        static QString recordingFilePath(const QString &fileName);
        static QString scriptFilePath(const QString &fileName);

        static void configureQSettings();
        static bool isDriveCPath(const QString &path);

    private:
        static QString ensureDirectory(const QString &path);
    };

} // namespace est

#endif // EST_APP_PATHS_H
