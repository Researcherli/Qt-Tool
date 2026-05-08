#ifndef EST_RECENTRECORDMANAGER_H
#define EST_RECENTRECORDMANAGER_H

#include <QObject>
#include <QVariantList>
#include <QMutex>

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
        QVariantList recentComparePairs() const;

        void addSerialProfile(const QString &portName,
                              int baudRate,
                              const QString &dataFormat);

        void addBinFile(const QString &filePath,
                        qint64 fileSize);

        void addSearchKeyword(const QString &keyword,
                              const QString &type);

        void addComparePair(const QString &leftPath, const QString &rightPath,
                            qint64 leftSize, qint64 rightSize);

        void clearAll();

    private:
        QVariantList recordsForKey(const QString &key) const;
        void appendRecord(const QString &key,
                          const QVariantMap &record);
        void appendCompareRecord(const QVariantMap &record);

        mutable QMutex m_mutex;  // 保护 JSON 文件的并发读写
    };

} // namespace est

#endif // EST_RECENTRECORDMANAGER_H
