#ifndef EST_RECENTRECORDMANAGER_H
#define EST_RECENTRECORDMANAGER_H

#include <QObject>
#include <QVariantList>

namespace est
{

    class RecentRecordManager : public QObject
    {
        Q_OBJECT
    public:
        explicit RecentRecordManager(QObject *parent = nullptr);

        QVariantList recentSerialConfigs() const;
        QVariantList recentBinFiles() const;
        QVariantList recentSearchKeywords() const;

        void addSerialProfile(const QString &portName,
                              int baudRate,
                              const QString &dataFormat);

        void addBinFile(const QString &filePath,
                        qint64 fileSize);

        void addSearchKeyword(const QString &keyword,
                              const QString &type);

        void clearAll();

    private:
        QVariantList recordsForKey(const QString &key) const;
        void appendRecord(const QString &key,
                          const QVariantMap &record);
    };

} // namespace est

#endif // EST_RECENTRECORDMANAGER_H
