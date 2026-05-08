#include "services/RecentRecordManager.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMutexLocker>
#include <QStandardPaths>

namespace est
{

    namespace
    {

        QString storagePath()
        {
            QString basePath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
            if (basePath.isEmpty())
            {
                basePath = QDir::homePath() + QStringLiteral("/EmbeddedSoftwareTools");
            }

            QDir dir(basePath);
            dir.mkpath(QStringLiteral("."));
            return dir.filePath(QStringLiteral("recent_records.json"));
        }

        QJsonObject loadRoot()
        {
            QFile file(storagePath());
            if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
            {
                return {};
            }

            const QJsonDocument document = QJsonDocument::fromJson(file.readAll());
            return document.isObject() ? document.object() : QJsonObject();
        }

        void saveRoot(const QJsonObject &root)
        {
            QFile file(storagePath());
            if (file.open(QIODevice::WriteOnly | QIODevice::Text))
            {
                file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
            }
        }

        QVariantList toVariantList(const QJsonArray &array)
        {
            QVariantList list;
            list.reserve(array.size());
            for (const QJsonValue &value : array)
            {
                list.append(value.toObject().toVariantMap());
            }
            return list;
        }

    } // namespace

    RecentRecordManager::RecentRecordManager(QObject *parent)
        : QObject(parent)
    {
    }

    QVariantList RecentRecordManager::recentSerialConfigs() const
    {
        QMutexLocker locker(&m_mutex);
        return recordsForKey(QStringLiteral("serialProfiles"));
    }

    QVariantList RecentRecordManager::recentBinFiles() const
    {
        QMutexLocker locker(&m_mutex);
        return recordsForKey(QStringLiteral("binFiles"));
    }

    QVariantList RecentRecordManager::recentSearchKeywords() const
    {
        QMutexLocker locker(&m_mutex);
        return recordsForKey(QStringLiteral("searchKeywords"));
    }

    void RecentRecordManager::addSerialProfile(const QString &portName,
                                               int baudRate,
                                               const QString &dataFormat)
    {
        QMutexLocker locker(&m_mutex);
        QVariantMap record;
        record.insert(QStringLiteral("portName"), portName);
        record.insert(QStringLiteral("baudRate"), baudRate);
        record.insert(QStringLiteral("dataFormat"), dataFormat);
        record.insert(QStringLiteral("timestamp"), QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm")));
        appendRecord(QStringLiteral("serialProfiles"), record);
    }

    void RecentRecordManager::addBinFile(const QString &filePath,
                                         qint64 fileSize)
    {
        QMutexLocker locker(&m_mutex);
        QFileInfo info(filePath);
        QVariantMap record;
        record.insert(QStringLiteral("fileName"), info.fileName());
        record.insert(QStringLiteral("filePath"), filePath);
        record.insert(QStringLiteral("fileSize"), fileSize);
        record.insert(QStringLiteral("timestamp"), QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm")));
        appendRecord(QStringLiteral("binFiles"), record);
    }

    void RecentRecordManager::addSearchKeyword(const QString &keyword,
                                               const QString &type)
    {
        QMutexLocker locker(&m_mutex);
        QVariantMap record;
        record.insert(QStringLiteral("keyword"), keyword);
        record.insert(QStringLiteral("type"), type);
        record.insert(QStringLiteral("timestamp"), QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm")));
        appendRecord(QStringLiteral("searchKeywords"), record);
    }

    QVariantList RecentRecordManager::recentComparePairs() const
    {
        QMutexLocker locker(&m_mutex);
        return recordsForKey(QStringLiteral("comparePairs"));
    }

    void RecentRecordManager::addComparePair(const QString &leftPath, const QString &rightPath,
                                              qint64 leftSize, qint64 rightSize)
    {
        QMutexLocker locker(&m_mutex);
        QFileInfo leftInfo(leftPath);
        QFileInfo rightInfo(rightPath);
        QVariantMap record;
        record.insert(QStringLiteral("leftPath"), leftPath);
        record.insert(QStringLiteral("rightPath"), rightPath);
        record.insert(QStringLiteral("leftName"), leftInfo.fileName());
        record.insert(QStringLiteral("rightName"), rightInfo.fileName());
        record.insert(QStringLiteral("leftSize"), leftSize);
        record.insert(QStringLiteral("rightSize"), rightSize);
        record.insert(QStringLiteral("timestamp"), QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm")));
        appendCompareRecord(record);
    }

    void RecentRecordManager::clearAll()
    {
        QMutexLocker locker(&m_mutex);
        saveRoot(QJsonObject());
    }

    QVariantList RecentRecordManager::recordsForKey(const QString &key) const
    {
        return toVariantList(loadRoot().value(key).toArray());
    }

    void RecentRecordManager::appendRecord(const QString &key,
                                           const QVariantMap &record)
    {
        QJsonObject root = loadRoot();
        QJsonArray array = root.value(key).toArray();

        QJsonArray filtered;
        for (const QJsonValue &value : array)
        {
            const QJsonObject object = value.toObject();
            const bool duplicatedSerial = key == QStringLiteral("serialProfiles")
                                          && object.value(QStringLiteral("portName")).toString() == record.value(QStringLiteral("portName")).toString()
                                          && object.value(QStringLiteral("baudRate")).toInt() == record.value(QStringLiteral("baudRate")).toInt();
            const bool duplicatedFile = key == QStringLiteral("binFiles")
                                        && object.value(QStringLiteral("filePath")).toString() == record.value(QStringLiteral("filePath")).toString();
            const bool duplicatedSearch = key == QStringLiteral("searchKeywords")
                                          && object.value(QStringLiteral("keyword")).toString() == record.value(QStringLiteral("keyword")).toString()
                                          && object.value(QStringLiteral("type")).toString() == record.value(QStringLiteral("type")).toString();
            if (!duplicatedSerial && !duplicatedFile && !duplicatedSearch)
            {
                filtered.append(object);
            }
        }

        filtered.prepend(QJsonObject::fromVariantMap(record));
        while (filtered.size() > 10)
        {
            filtered.removeLast();
        }

        root.insert(key, filtered);
        saveRoot(root);
    }

    void RecentRecordManager::appendCompareRecord(const QVariantMap &record)
    {
        const QString key = QStringLiteral("comparePairs");
        QJsonObject root = loadRoot();
        QJsonArray array = root.value(key).toArray();

        QJsonArray filtered;
        const QString newLeftPath = record.value(QStringLiteral("leftPath")).toString();
        const QString newRightPath = record.value(QStringLiteral("rightPath")).toString();
        for (const QJsonValue &value : array)
        {
            const QJsonObject object = value.toObject();
            const bool duplicated = object.value(QStringLiteral("leftPath")).toString() == newLeftPath
                                    && object.value(QStringLiteral("rightPath")).toString() == newRightPath;
            if (!duplicated)
            {
                filtered.append(object);
            }
        }

        filtered.prepend(QJsonObject::fromVariantMap(record));
        while (filtered.size() > 10)
        {
            filtered.removeLast();
        }

        root.insert(key, filtered);
        saveRoot(root);
    }

} // namespace est
