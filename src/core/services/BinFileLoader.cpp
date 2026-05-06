#include "services/BinFileLoader.h"

#include <QFile>
#include <QFileInfo>

namespace est
{

    bool BinFileLoader::loadFile(const QString &filePath, QString *errorMessage)
    {
        QFile file(filePath);
        if (!file.open(QIODevice::ReadOnly))
        {
            if (errorMessage != nullptr)
            {
                *errorMessage = QStringLiteral("无法打开 BIN 文件：%1").arg(file.errorString());
            }
            return false;
        }

        m_data = file.readAll();
        m_filePath = filePath;
        return true;
    }

    bool BinFileLoader::reload(QString *errorMessage)
    {
        if (m_filePath.isEmpty())
        {
            if (errorMessage != nullptr)
            {
                *errorMessage = QStringLiteral("当前没有已加载的 BIN 文件。");
            }
            return false;
        }
        return loadFile(m_filePath, errorMessage);
    }

    void BinFileLoader::clear()
    {
        m_filePath.clear();
        m_data.clear();
    }

    bool BinFileLoader::isLoaded() const
    {
        return !m_filePath.isEmpty();
    }

    QString BinFileLoader::filePath() const
    {
        return m_filePath;
    }

    QString BinFileLoader::fileName() const
    {
        return QFileInfo(m_filePath).fileName();
    }

    qint64 BinFileLoader::fileSize() const
    {
        return m_data.size();
    }

    const QByteArray &BinFileLoader::data() const
    {
        return m_data;
    }

} // namespace est
