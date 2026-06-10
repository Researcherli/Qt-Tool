#include "services/SlcanCodec.h"

#include <QRegularExpression>

namespace est
{

    namespace
    {
        void setError(QString *errorMessage, const QString &text)
        {
            if (errorMessage != nullptr)
            {
                *errorMessage = text;
            }
        }
    }

    QByteArray SlcanCodec::bitrateCommand(int bitrate)
    {
        switch (bitrate)
        {
        case 10000:
            return QByteArrayLiteral("S0");
        case 20000:
            return QByteArrayLiteral("S1");
        case 50000:
            return QByteArrayLiteral("S2");
        case 100000:
            return QByteArrayLiteral("S3");
        case 125000:
            return QByteArrayLiteral("S4");
        case 250000:
            return QByteArrayLiteral("S5");
        case 500000:
            return QByteArrayLiteral("S6");
        case 800000:
            return QByteArrayLiteral("S7");
        case 1000000:
            return QByteArrayLiteral("S8");
        default:
            return {};
        }
    }

    bool SlcanCodec::isSupportedBitrate(int bitrate)
    {
        return !bitrateCommand(bitrate).isEmpty();
    }

    bool SlcanCodec::parseFrameLine(const QString &line, CanFrame *frame)
    {
        if (frame == nullptr)
        {
            return false;
        }

        static const QRegularExpression s_standardData(
            QStringLiteral("^t([0-9a-fA-F]{3})([0-8])([0-9a-fA-F]{0,16})$"));
        static const QRegularExpression s_extendedData(
            QStringLiteral("^T([0-9a-fA-F]{8})([0-8])([0-9a-fA-F]{0,16})$"));
        static const QRegularExpression s_standardRemote(
            QStringLiteral("^r([0-9a-fA-F]{3})([0-8])$"));
        static const QRegularExpression s_extendedRemote(
            QStringLiteral("^R([0-9a-fA-F]{8})([0-8])$"));

        const QString trimmed = line.trimmed();
        QRegularExpressionMatch match;
        bool extended = false;
        bool remote = false;

        // 依次匹配：标准数据帧 → 扩展数据帧 → 标准远程帧 → 扩展远程帧
        // 每轮匹配都重置 extended/remote 标志，防止前一匹配遗留的标志干扰
        match = s_standardData.match(trimmed);
        if (match.hasMatch())
        {
            extended = false;
            remote = false;
        }
        else
        {
            match = s_extendedData.match(trimmed);
            if (match.hasMatch())
            {
                extended = true;
                remote = false;
            }
            else
            {
                match = s_standardRemote.match(trimmed);
                if (match.hasMatch())
                {
                    extended = false;
                    remote = true;
                }
                else
                {
                    match = s_extendedRemote.match(trimmed);
                    if (match.hasMatch())
                    {
                        extended = true;
                        remote = true;
                    }
                    else
                    {
                        return false;
                    }
                }
            }
        }

        bool ok = false;
        const quint32 id = match.captured(1).toUInt(&ok, 16);
        if (!ok)
        {
            return false;
        }

        CanFrame parsed;
        parsed.id = id;
        parsed.dlc = match.captured(2).toInt();
        parsed.extended = extended;
        parsed.remote = remote;
        if (!remote)
        {
            parsed.data = QByteArray::fromHex(match.captured(3).toLatin1());
            if (parsed.data.size() != parsed.dlc)
            {
                return false;
            }
        }

        *frame = parsed;
        return true;
    }

    QByteArray SlcanCodec::buildDataFrame(quint32 id, int dlc, const QByteArray &data, bool extended)
    {
        const QString dataHex = QString::fromLatin1(data.toHex().toUpper());
        if (extended)
        {
            return QStringLiteral("T%1%2%3\r")
                .arg(id, 8, 16, QLatin1Char('0'))
                .arg(dlc)
                .arg(dataHex)
                .toLatin1();
        }

        return QStringLiteral("t%1%2%3\r")
            .arg(id, 3, 16, QLatin1Char('0'))
            .arg(dlc)
            .arg(dataHex)
            .toLatin1();
    }

    bool SlcanCodec::parseHexPayload(const QString &text, QByteArray *payload, QString *errorMessage)
    {
        if (payload == nullptr)
        {
            setError(errorMessage, QStringLiteral("内部错误：输出缓冲为空"));
            return false;
        }

        QString compact = text.simplified();
        compact.remove(QLatin1Char(' '));
        compact.remove(QLatin1Char('_'));
        compact.remove(QStringLiteral("0x"), Qt::CaseInsensitive);

        if (compact.isEmpty())
        {
            payload->clear();
            return true;
        }

        static const QRegularExpression s_hex(QStringLiteral("^[0-9a-fA-F]+$"));
        if (!s_hex.match(compact).hasMatch())
        {
            setError(errorMessage, QStringLiteral("数据区只能包含十六进制字符、空格、下划线或 0x 前缀"));
            return false;
        }
        if ((compact.size() % 2) != 0)
        {
            setError(errorMessage, QStringLiteral("数据区十六进制字符数量必须为偶数"));
            return false;
        }

        *payload = QByteArray::fromHex(compact.toLatin1());
        return true;
    }

    bool SlcanCodec::validateDataFrame(quint32 id, int dlc, const QByteArray &data, bool extended,
                                       QString *errorMessage)
    {
        if (dlc < 0 || dlc > 8)
        {
            setError(errorMessage, QStringLiteral("经典 CAN 的 DLC 范围是 0 ~ 8"));
            return false;
        }
        if (!extended && id > 0x7FF)
        {
            setError(errorMessage, QStringLiteral("标准帧 ID 范围是 0x000 ~ 0x7FF"));
            return false;
        }
        if (extended && id > 0x1FFFFFFF)
        {
            setError(errorMessage, QStringLiteral("扩展帧 ID 范围是 0x00000000 ~ 0x1FFFFFFF"));
            return false;
        }
        if (data.size() != dlc)
        {
            setError(errorMessage, QStringLiteral("数据字节数必须等于 DLC"));
            return false;
        }

        return true;
    }

} // namespace est
