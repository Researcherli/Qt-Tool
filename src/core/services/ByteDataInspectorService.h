#ifndef EST_BYTEDATAINSPECTORSERVICE_H
#define EST_BYTEDATAINSPECTORSERVICE_H

#include <QByteArrayView>
#include <QMap>
#include <QString>

namespace est
{

    class ByteDataInspectorService
    {
    public:
        enum class Endian
        {
            Little,
            Big
        };

        static QMap<QString, QString> inspect(QByteArrayView data, qsizetype offset, Endian endian);
    };

} // namespace est

#endif // EST_BYTEDATAINSPECTORSERVICE_H
