#ifndef EST_SLCANCODEC_H
#define EST_SLCANCODEC_H

#include <QByteArray>
#include <QString>

namespace est
{

    struct CanFrame
    {
        quint32 id = 0;
        int dlc = 0;
        QByteArray data;
        bool extended = false;
        bool remote = false;
    };

    class SlcanCodec
    {
    public:
        static QByteArray bitrateCommand(int bitrate);
        static bool isSupportedBitrate(int bitrate);

        static bool parseFrameLine(const QString &line, CanFrame *frame);
        static QByteArray buildDataFrame(quint32 id, int dlc, const QByteArray &data, bool extended);

        static bool parseHexPayload(const QString &text, QByteArray *payload, QString *errorMessage = nullptr);
        static bool validateDataFrame(quint32 id, int dlc, const QByteArray &data, bool extended,
                                      QString *errorMessage = nullptr);
    };

} // namespace est

#endif // EST_SLCANCODEC_H
