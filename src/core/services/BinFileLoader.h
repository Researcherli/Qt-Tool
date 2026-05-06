#ifndef EST_BINFILELOADER_H
#define EST_BINFILELOADER_H

#include <QByteArray>
#include <QString>

namespace est
{

    class BinFileLoader
    {
    public:
        bool loadFile(const QString &filePath, QString *errorMessage = nullptr);
        bool reload(QString *errorMessage = nullptr);
        void clear();

        bool isLoaded() const;
        QString filePath() const;
        QString fileName() const;
        qint64 fileSize() const;
        const QByteArray &data() const;

    private:
        QString m_filePath;
        QByteArray m_data;
    };

} // namespace est

#endif // EST_BINFILELOADER_H
