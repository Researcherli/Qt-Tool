#include "services/ProtocolTemplate.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QSettings>

namespace est
{

    // ---------------------------------------------------------------------------
    // 工具函数
    // ---------------------------------------------------------------------------

    QString fieldTypeToString(FieldType type)
    {
        switch (type)
        {
        case FieldType::Constant:
            return QStringLiteral("常量");
        case FieldType::Length:
            return QStringLiteral("长度");
        case FieldType::Type:
            return QStringLiteral("类型");
        case FieldType::Address:
            return QStringLiteral("地址");
        case FieldType::Payload:
            return QStringLiteral("负载");
        case FieldType::Checksum:
            return QStringLiteral("校验");
        case FieldType::Sequence:
            return QStringLiteral("序号");
        case FieldType::Status:
            return QStringLiteral("状态");
        case FieldType::Reserved:
            return QStringLiteral("保留");
        case FieldType::Custom:
            return QStringLiteral("自定义");
        }
        return QStringLiteral("未知");
    }

    QString fieldDisplayToString(FieldDisplay display)
    {
        switch (display)
        {
        case FieldDisplay::Hex:
            return QStringLiteral("HEX");
        case FieldDisplay::Dec:
            return QStringLiteral("DEC");
        case FieldDisplay::Signed:
            return QStringLiteral("有符号");
        case FieldDisplay::Bin:
            return QStringLiteral("BIN");
        case FieldDisplay::ASCII:
            return QStringLiteral("ASCII");
        case FieldDisplay::Float:
            return QStringLiteral("FLOAT");
        }
        return QStringLiteral("HEX");
    }

    // ---------------------------------------------------------------------------
    // FieldDef
    // ---------------------------------------------------------------------------

    static QString keyName() { return QStringLiteral("name"); }
    static QString keyDesc() { return QStringLiteral("description"); }
    static QString keyType() { return QStringLiteral("type"); }
    static QString keyOffset() { return QStringLiteral("offset"); }
    static QString keySize() { return QStringLiteral("size"); }
    static QString keyExpected() { return QStringLiteral("expectedValue"); }
    static QString keyDisplay() { return QStringLiteral("display"); }
    static QString keyColor() { return QStringLiteral("color"); }

    static QString typeToString(FieldType t)
    {
        switch (t)
        {
        case FieldType::Constant:
            return QStringLiteral("constant");
        case FieldType::Length:
            return QStringLiteral("length");
        case FieldType::Type:
            return QStringLiteral("type");
        case FieldType::Address:
            return QStringLiteral("address");
        case FieldType::Payload:
            return QStringLiteral("payload");
        case FieldType::Checksum:
            return QStringLiteral("checksum");
        case FieldType::Sequence:
            return QStringLiteral("sequence");
        case FieldType::Status:
            return QStringLiteral("status");
        case FieldType::Reserved:
            return QStringLiteral("reserved");
        case FieldType::Custom:
            return QStringLiteral("custom");
        }
        return QStringLiteral("custom");
    }

    static FieldType typeFromString(const QString &s)
    {
        if (s == QStringLiteral("constant"))
            return FieldType::Constant;
        if (s == QStringLiteral("length"))
            return FieldType::Length;
        if (s == QStringLiteral("type"))
            return FieldType::Type;
        if (s == QStringLiteral("address"))
            return FieldType::Address;
        if (s == QStringLiteral("payload"))
            return FieldType::Payload;
        if (s == QStringLiteral("checksum"))
            return FieldType::Checksum;
        if (s == QStringLiteral("sequence"))
            return FieldType::Sequence;
        if (s == QStringLiteral("status"))
            return FieldType::Status;
        if (s == QStringLiteral("reserved"))
            return FieldType::Reserved;
        return FieldType::Custom;
    }

    static QString displayToString(FieldDisplay d)
    {
        switch (d)
        {
        case FieldDisplay::Hex:
            return QStringLiteral("hex");
        case FieldDisplay::Dec:
            return QStringLiteral("dec");
        case FieldDisplay::Signed:
            return QStringLiteral("signed");
        case FieldDisplay::Bin:
            return QStringLiteral("bin");
        case FieldDisplay::ASCII:
            return QStringLiteral("ascii");
        case FieldDisplay::Float:
            return QStringLiteral("float");
        }
        return QStringLiteral("hex");
    }

    static FieldDisplay displayFromString(const QString &s)
    {
        if (s == QStringLiteral("dec"))
            return FieldDisplay::Dec;
        if (s == QStringLiteral("signed"))
            return FieldDisplay::Signed;
        if (s == QStringLiteral("bin"))
            return FieldDisplay::Bin;
        if (s == QStringLiteral("ascii"))
            return FieldDisplay::ASCII;
        if (s == QStringLiteral("float"))
            return FieldDisplay::Float;
        return FieldDisplay::Hex;
    }

    QJsonObject FieldDef::toJson() const
    {
        QJsonObject obj;
        obj[keyName()] = name;
        obj[keyDesc()] = description;
        obj[keyType()] = typeToString(type);
        obj[keyOffset()] = offset;
        obj[keySize()] = size;
        obj[keyExpected()] = QString::fromLatin1(expectedValue.toHex());
        obj[keyDisplay()] = displayToString(display);
        obj[keyColor()] = highlightColor.name();
        return obj;
    }

    FieldDef FieldDef::fromJson(const QJsonObject &obj)
    {
        FieldDef def;
        def.name = obj.value(keyName()).toString();
        def.description = obj.value(keyDesc()).toString();
        def.type = typeFromString(obj.value(keyType()).toString());
        def.offset = obj.value(keyOffset()).toInt(0);
        def.size = obj.value(keySize()).toInt(1);
        const QString hexStr = obj.value(keyExpected()).toString();
        if (!hexStr.isEmpty())
            def.expectedValue = QByteArray::fromHex(hexStr.toLatin1());
        def.display = displayFromString(obj.value(keyDisplay()).toString());
        const QString colorStr = obj.value(keyColor()).toString();
        if (!colorStr.isEmpty())
            def.highlightColor = QColor(colorStr);
        return def;
    }

    // ---------------------------------------------------------------------------
    // ProtocolTemplate
    // ---------------------------------------------------------------------------

    bool ProtocolTemplate::isValid() const
    {
        if (name.trimmed().isEmpty())
            return false;
        if (fields.isEmpty())
            return false;
        if (maxFrameSize <= 0)
            return false;

        int checksumCount = 0;
        for (const auto &f : fields)
        {
            if (f.name.trimmed().isEmpty())
                return false;
            if (f.offset < 0 && f.type != FieldType::Checksum)
                return false;
            if (f.size < 0)
                return false;
            if (f.size == 0 && f.type != FieldType::Payload)
                return false;
            if (f.type == FieldType::Constant
                && !f.expectedValue.isEmpty()
                && f.size > 0
                && f.expectedValue.size() != f.size)
            {
                return false;
            }
            if (f.type == FieldType::Checksum)
                ++checksumCount;
        }
        return checksumCount <= 1;
    }

    QJsonObject ProtocolTemplate::toJson() const
    {
        QJsonObject obj;
        obj[QStringLiteral("name")] = name;
        obj[QStringLiteral("description")] = description;
        obj[QStringLiteral("maxFrameSize")] = maxFrameSize;
        obj[QStringLiteral("endian")] = (endian == ByteDataInspectorService::Endian::Little)
                                            ? QStringLiteral("little")
                                            : QStringLiteral("big");
        obj[QStringLiteral("checksumAlgo")] = checksumAlgo;

        QJsonArray arr;
        for (const auto &f : fields)
            arr.append(f.toJson());
        obj[QStringLiteral("fields")] = arr;

        return obj;
    }

    ProtocolTemplate ProtocolTemplate::fromJson(const QJsonObject &obj)
    {
        ProtocolTemplate tmpl;
        tmpl.name = obj.value(QStringLiteral("name")).toString();
        tmpl.description = obj.value(QStringLiteral("description")).toString();
        tmpl.maxFrameSize = obj.value(QStringLiteral("maxFrameSize")).toInt(256);
        tmpl.endian = (obj.value(QStringLiteral("endian")).toString() == QStringLiteral("big"))
                          ? ByteDataInspectorService::Endian::Big
                          : ByteDataInspectorService::Endian::Little;
        tmpl.checksumAlgo = obj.value(QStringLiteral("checksumAlgo")).toString();

        const QJsonArray arr = obj.value(QStringLiteral("fields")).toArray();
        for (const auto &v : arr)
            tmpl.fields.append(FieldDef::fromJson(v.toObject()));

        return tmpl;
    }

    void ProtocolTemplate::saveList(const QVector<ProtocolTemplate> &list)
    {
        QJsonArray arr;
        for (const auto &t : list)
            arr.append(t.toJson());

        const QByteArray jsonData = QJsonDocument(arr).toJson(QJsonDocument::Compact);
        if (jsonData.size() > 500 * 1024) {  // 500KB warning threshold
            qWarning() << "Protocol template data is large (" << jsonData.size() << "bytes)."
                        << "Consider using file-based storage instead of QSettings.";
        }

        QSettings settings(QStringLiteral("EST"), QStringLiteral("ProtocolDecoder"));
        settings.setValue(QStringLiteral("templates"),
                          QString::fromUtf8(jsonData));
    }

    QVector<ProtocolTemplate> ProtocolTemplate::loadList()
    {
        QVector<ProtocolTemplate> result;

        QSettings settings(QStringLiteral("EST"), QStringLiteral("ProtocolDecoder"));
        const QString jsonStr = settings.value(QStringLiteral("templates")).toString();
        if (jsonStr.isEmpty())
            return result;

        QJsonParseError err;
        const QJsonDocument doc = QJsonDocument::fromJson(jsonStr.toUtf8(), &err);
        if (err.error != QJsonParseError::NoError || !doc.isArray())
            return result;

        const QJsonArray arr = doc.array();
        for (const auto &v : arr)
        {
            ProtocolTemplate tmpl = ProtocolTemplate::fromJson(v.toObject());
            if (tmpl.isValid())
                result.append(tmpl);
        }

        return result;
    }

} // namespace est
