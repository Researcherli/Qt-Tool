#ifndef EST_ENCODINGUTIL_H
#define EST_ENCODINGUTIL_H

#include <QByteArray>
#include <QString>

namespace est
{

    /**
     * 编码工具 — 提供跨模块共享的编码/解码、Hex 格式化等工具函数。
     *
     * 将 ByteFormatService 和 DataConvertService 中重复的代码集中于此。
     */
    class EncodingUtil
    {
    public:
        // ---- GBK 编解码 ----

        /// GBK → UTF-16 (QString)，非 Windows 平台降级为 fromLocal8Bit
        static QString decodeGbk(const QByteArray &bytes,
                                  bool *ok = nullptr,
                                  QString *errorMessage = nullptr);

        /// UTF-16 (QString) → GBK，非 Windows 平台降级为 toLocal8Bit
        static QByteArray encodeGbk(const QString &text,
                                      bool *ok = nullptr,
                                      QString *errorMessage = nullptr);

        // ---- UTF-16 编解码 ----

        static QString decodeUtf16(const QByteArray &bytes, bool bigEndian,
                                    bool *ok = nullptr,
                                    QString *errorMessage = nullptr);

        static QByteArray encodeUtf16(const QString &text, bool bigEndian);

        // ---- Hex 输入规范化 ----

        /// 移除 0x/0X/\x 前缀、空格、逗号、分号、花括号、方括号
        static QString normalizedHexInput(QString input);

        // ---- C 数组名检查 ----

        /// 验证是否为合法 C 标识符，否则返回 "data"
        static QString sanitizedArrayName(const QString &arrayName);

        // ---- Hex 值格式化 ----

        /// 统一构造 `0x%1` 格式（不经过 toUpper/replace 绕弯）
        static QString hexPrefix(quint64 value, int digits);

        static QString hexPrefix(quint8 value);
        static QString hexPrefix(quint16 value);
        static QString hexPrefix(quint32 value);
    };

} // namespace est

#endif // EST_ENCODINGUTIL_H
