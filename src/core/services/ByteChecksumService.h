#ifndef EST_BYTECHECKSUMSERVICE_H
#define EST_BYTECHECKSUMSERVICE_H

#include <QByteArrayView>
#include <QMap>
#include <QString>

namespace est
{

    class ByteChecksumService
    {
    public:
        static QMap<QString, QString> calculate(QByteArrayView data);
    };

} // namespace est

#endif // EST_BYTECHECKSUMSERVICE_H
