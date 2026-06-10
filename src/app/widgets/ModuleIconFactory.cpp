#include "widgets/ModuleIconFactory.h"

#include <QPainter>
#include <QPainterPath>

namespace est
{
    QPixmap makeModuleIconPixmap(const QString &type, int size)
    {
        QPixmap pixmap(size, size);
        pixmap.fill(Qt::transparent);

        QPainter p(&pixmap);
        p.setRenderHint(QPainter::Antialiasing);

        if (type == QStringLiteral("serial"))
        {
            const QRectF screen(size * 0.16, size * 0.22, size * 0.68, size * 0.44);
            QPainterPath monitor;
            monitor.addRoundedRect(screen, size * 0.05, size * 0.05);
            p.fillPath(monitor, QColor(QStringLiteral("#2F7FB5")));
            p.fillRect(screen.adjusted(size * 0.08, size * 0.08, -size * 0.08, -size * 0.1),
                       QColor(QStringLiteral("#EAF2FA")));
            p.setPen(QPen(QColor(QStringLiteral("#173E63")), qMax(2, size / 26)));
            p.drawPath(monitor);
            p.setBrush(QColor(QStringLiteral("#2F7FB5")));
            p.setPen(Qt::NoPen);
            p.drawRoundedRect(QRectF(size * 0.42, size * 0.66, size * 0.16, size * 0.1), size * 0.02, size * 0.02);
            p.drawRoundedRect(QRectF(size * 0.32, size * 0.76, size * 0.36, size * 0.06), size * 0.02, size * 0.02);
        }
        else if (type == QStringLiteral("record_start"))
        {
            p.setPen(Qt::NoPen);
            p.setBrush(QColor(QStringLiteral("#2EAD4F")));
            p.drawEllipse(QPointF(size / 2.0, size / 2.0), size * 0.34, size * 0.34);

            QPainterPath triangle;
            triangle.moveTo(size * 0.43, size * 0.34);
            triangle.lineTo(size * 0.43, size * 0.66);
            triangle.lineTo(size * 0.67, size * 0.50);
            triangle.closeSubpath();
            p.fillPath(triangle, QColor(QStringLiteral("#FFFFFF")));
        }
        else if (type == QStringLiteral("record_stop"))
        {
            p.setPen(Qt::NoPen);
            p.setBrush(QColor(QStringLiteral("#D32F2F")));
            p.drawEllipse(QPointF(size / 2.0, size / 2.0), size * 0.34, size * 0.34);
            p.setBrush(QColor(QStringLiteral("#FFFFFF")));
            p.drawRoundedRect(QRectF(size * 0.39, size * 0.39, size * 0.22, size * 0.22),
                              size * 0.025, size * 0.025);
        }
        else if (type == QStringLiteral("playback"))
        {
            p.setPen(Qt::NoPen);
            p.setBrush(QColor(QStringLiteral("#2F7FB5")));
            p.drawEllipse(QPointF(size / 2.0, size / 2.0), size * 0.32, size * 0.32);

            QPainterPath triangle;
            triangle.moveTo(size * 0.42, size * 0.35);
            triangle.lineTo(size * 0.42, size * 0.65);
            triangle.lineTo(size * 0.66, size * 0.50);
            triangle.closeSubpath();
            p.fillPath(triangle, QColor(QStringLiteral("#FFFFFF")));
        }
        else if (type == QStringLiteral("save"))
        {
            const QRectF body(size * 0.24, size * 0.18, size * 0.52, size * 0.64);
            QPainterPath disk;
            disk.addRoundedRect(body, size * 0.05, size * 0.05);
            p.fillPath(disk, QColor(QStringLiteral("#FFFFFF")));
            p.setPen(QPen(QColor(QStringLiteral("#173E63")), qMax(2, size / 28)));
            p.drawPath(disk);
            p.fillRect(QRectF(size * 0.34, size * 0.20, size * 0.28, size * 0.16),
                       QColor(QStringLiteral("#2F7FB5")));
            p.fillRect(QRectF(size * 0.34, size * 0.56, size * 0.32, size * 0.18),
                       QColor(QStringLiteral("#DCECF8")));
        }
        else if (type == QStringLiteral("convert"))
        {
            const QRectF doc(size * 0.26, size * 0.14, size * 0.48, size * 0.68);
            QPainterPath docPath;
            docPath.addRoundedRect(doc, size * 0.04, size * 0.04);
            p.fillPath(docPath, QColor(QStringLiteral("#FFFFFF")));
            p.setPen(QPen(QColor(QStringLiteral("#173E63")), qMax(2, size / 28)));
            p.drawPath(docPath);

            p.setPen(QPen(QColor(QStringLiteral("#2F7FB5")), qMax(2, size / 24), Qt::SolidLine, Qt::RoundCap));
            p.drawLine(QPointF(size * 0.36, size * 0.34), QPointF(size * 0.62, size * 0.34));
            p.drawLine(QPointF(size * 0.36, size * 0.48), QPointF(size * 0.62, size * 0.48));
            p.drawLine(QPointF(size * 0.36, size * 0.62), QPointF(size * 0.56, size * 0.62));
        }
        else if (type == QStringLiteral("compare"))
        {
            const QRectF docLeft(size * 0.18, size * 0.14, size * 0.3, size * 0.68);
            const QRectF docRight(size * 0.52, size * 0.14, size * 0.3, size * 0.68);
            QPainterPath leftPath, rightPath;
            leftPath.addRoundedRect(docLeft, size * 0.035, size * 0.035);
            rightPath.addRoundedRect(docRight, size * 0.035, size * 0.035);

            p.fillPath(leftPath, QColor(QStringLiteral("#FFFFFF")));
            p.fillPath(rightPath, QColor(QStringLiteral("#FFFFFF")));

            p.setPen(QPen(QColor(QStringLiteral("#173E63")), qMax(2, size / 26)));
            p.drawPath(leftPath);
            p.drawPath(rightPath);

            p.setPen(QPen(QColor(QStringLiteral("#2F7FB5")), qMax(2, size / 30), Qt::SolidLine, Qt::RoundCap));
            p.drawLine(QPointF(size * 0.24, size * 0.3), QPointF(size * 0.42, size * 0.3));
            p.drawLine(QPointF(size * 0.24, size * 0.45), QPointF(size * 0.42, size * 0.45));
            p.drawLine(QPointF(size * 0.24, size * 0.6), QPointF(size * 0.42, size * 0.6));
            p.drawLine(QPointF(size * 0.58, size * 0.3), QPointF(size * 0.76, size * 0.3));
            p.setPen(QPen(QColor(QStringLiteral("#D32F2F")), qMax(2, size / 30), Qt::SolidLine, Qt::RoundCap));
            p.drawLine(QPointF(size * 0.58, size * 0.45), QPointF(size * 0.76, size * 0.45));
            p.setPen(QPen(QColor(QStringLiteral("#2F7FB5")), qMax(2, size / 30), Qt::SolidLine, Qt::RoundCap));
            p.drawLine(QPointF(size * 0.58, size * 0.6), QPointF(size * 0.76, size * 0.6));
        }
        else if (type == QStringLiteral("waveform"))
        {
            Q_UNUSED(p)
            // 正弦波 + 坐标轴
            const int cx = size / 2;
            const int cy = size / 2;
            const int amp = size * 0.18;
            const int halfW = static_cast<int>(size * 0.3);

            // 坐标轴
            p.setPen(QPen(QColor(QStringLiteral("#90A4AE")), qMax(1, size / 36)));
            p.drawLine(QPointF(cx - halfW - size * 0.04, cy),
                       QPointF(cx + halfW + size * 0.04, cy));
            p.drawLine(QPointF(cx - halfW - size * 0.04, cy - amp - size * 0.06),
                       QPointF(cx - halfW - size * 0.04, cy + amp + size * 0.06));

            // 波形曲线
            QPainterPath wave;
            for (int x = -halfW; x <= halfW; ++x)
            {
                const double phase = (static_cast<double>(x + halfW) / (halfW * 2)) * 4.0 * M_PI;
                const double y = cy - amp * qSin(phase) - amp * 0.35 * qSin(phase * 0.6 + 1.2);
                if (x == -halfW)
                    wave.moveTo(cx + x, y);
                else
                    wave.lineTo(cx + x, y);
            }
            p.setPen(QPen(QColor(QStringLiteral("#4CAF50")), qMax(2, size / 18), Qt::SolidLine, Qt::RoundCap));
            p.setBrush(Qt::NoPen);
            p.drawPath(wave);
        }
        else if (type == QStringLiteral("rtos"))
        {
            // 芯片 + 任务列表图标
            const int cx = size / 2;
            const int half = static_cast<int>(size * 0.32);

            // 芯片主体
            QPainterPath chip;
            chip.addRoundedRect(
                QRectF(cx - half, cx - half, half * 2, half * 2),
                size * 0.06, size * 0.06);
            p.fillPath(chip, QColor(QStringLiteral("#2F7FB5")));

            // 芯片内部
            p.fillRect(QRectF(cx - half + size * 0.06, cx - half + size * 0.06,
                              half * 2 - size * 0.12, half * 2 - size * 0.12),
                       QColor(QStringLiteral("#DCECF8")));

            // 列表线条（3条水平线）
            p.setPen(QPen(QColor(QStringLiteral("#173E63")),
                          qMax(1, size / 28), Qt::SolidLine, Qt::RoundCap));
            const int lineYStart = cx - half + size * 0.18;
            const int lineSpacing = static_cast<int>(size * 0.2);
            const int lineLeft = cx - half + size * 0.14;
            const int lineWidth = static_cast<int>(half * 2 - size * 0.28);
            for (int i = 0; i < 3; ++i)
            {
                const int ly = lineYStart + i * lineSpacing;
                p.drawLine(QPointF(lineLeft, ly),
                           QPointF(lineLeft + lineWidth, ly));
            }

            // 引脚（四周小点）
            p.setBrush(QColor(QStringLiteral("#173E63")));
            p.setPen(Qt::NoPen);
            const int pinSize = qMax(2, size / 16);
            const int pinOffset = static_cast<int>(size * 0.02);
            // 顶部引脚
            p.drawRect(QRectF(cx - half + size * 0.08, cx - half - pinOffset,
                              pinSize, pinSize));
            p.drawRect(QRectF(cx - size * 0.04, cx - half - pinOffset,
                              pinSize, pinSize));
            p.drawRect(QRectF(cx + half - size * 0.12, cx - half - pinOffset,
                              pinSize, pinSize));
            // 底部引脚
            p.drawRect(QRectF(cx - half + size * 0.08, cx + half - pinOffset,
                              pinSize, pinSize));
            p.drawRect(QRectF(cx - size * 0.04, cx + half - pinOffset,
                              pinSize, pinSize));
            p.drawRect(QRectF(cx + half - size * 0.12, cx + half - pinOffset,
                              pinSize, pinSize));
        }
        else if (type == QStringLiteral("protocol"))
        {
            // 数据包 + 字段分解图标
            const int cx = size / 2;
            const int half = static_cast<int>(size * 0.30);
            const int by = cx + half;

            // 数据包矩形
            QPainterPath packet;
            packet.addRoundedRect(
                QRectF(cx - half, cx - half, half * 2, half * 2),
                size * 0.05, size * 0.05);
            p.fillPath(packet, QColor(QStringLiteral("#FFFFFF")));
            p.setPen(QPen(QColor(QStringLiteral("#173E63")), qMax(1, size / 28)));
            p.drawPath(packet);

            // 帧头(橙色)条
            p.setBrush(QColor(QStringLiteral("#FF9800")));
            p.setPen(Qt::NoPen);
            p.drawRoundedRect(
                QRectF(cx - half + size * 0.06, cx - half + size * 0.08,
                       half * 2 - size * 0.12, size * 0.08),
                size * 0.02, size * 0.02);

            // 数据字段(蓝色)条
            p.setBrush(QColor(QStringLiteral("#42A5F5")));
            p.drawRoundedRect(
                QRectF(cx - half + size * 0.06, cx - half + size * 0.22,
                       half * 2 - size * 0.12, size * 0.08),
                size * 0.02, size * 0.02);

            // 更多字段(浅蓝)
            p.setBrush(QColor(QStringLiteral("#90CAF9")));
            p.drawRoundedRect(
                QRectF(cx - half + size * 0.06, cx - half + size * 0.36,
                       half * 2 - size * 0.12, size * 0.08),
                size * 0.02, size * 0.02);

            // 校验(绿色)
            p.setBrush(QColor(QStringLiteral("#66BB6A")));
            p.drawRoundedRect(
                QRectF(cx - half + size * 0.06, cx - half + size * 0.50,
                       half * 2 - size * 0.12, size * 0.08),
                size * 0.02, size * 0.02);

            // 放大镜/解析符号（右侧小圆+弧线）
            p.setPen(QPen(QColor(QStringLiteral("#2F7FB5")), qMax(2, size / 20)));
            p.setBrush(Qt::NoPen);
            const int lensCx = cx + half + size * 0.06;
            const int lensCy = by - size * 0.10;
            p.drawEllipse(QPointF(lensCx, lensCy), size * 0.06, size * 0.06);
            p.drawLine(QPointF(lensCx + size * 0.06, lensCy + size * 0.06),
                       QPointF(lensCx + size * 0.12, lensCy + size * 0.12));
        }
        else if (type == QStringLiteral("terminal"))
        {
            // 终端/控制台图标
            const int cx = size / 2;
            const int half = static_cast<int>(size * 0.30);
            const int ch = static_cast<int>(size * 0.36);

            // 显示器/终端框
            QPainterPath screen;
            screen.addRoundedRect(
                QRectF(cx - half, cx - ch, half * 2, ch * 1.6),
                size * 0.04, size * 0.04);
            p.fillPath(screen, QColor(QStringLiteral("#1E1E1E")));
            p.setPen(QPen(QColor(QStringLiteral("#2F7FB5")), qMax(1, size / 30)));
            p.drawPath(screen);

            // 提示符 > _
            p.setPen(QPen(QColor(QStringLiteral("#4CAF50")), qMax(2, size / 22)));
            p.drawText(QRectF(cx - half + size * 0.08, cx - ch + size * 0.08,
                              half * 2 - size * 0.16, ch * 1.6 - size * 0.16),
                       Qt::AlignVCenter | Qt::AlignLeft,
                       QStringLiteral(">_"));

            // 底部底座
            p.setBrush(QColor(QStringLiteral("#173E63")));
            p.setPen(Qt::NoPen);
            p.drawRoundedRect(
                QRectF(cx - size * 0.12, cx + ch - size * 0.04,
                       size * 0.24, size * 0.10),
                size * 0.02, size * 0.02);
            p.drawRoundedRect(
                QRectF(cx - size * 0.08, cx + ch + size * 0.06,
                       size * 0.16, size * 0.06),
                size * 0.02, size * 0.02);
        }
        else if (type == QStringLiteral("can"))
        {
            // CAN 总线图标 — 节点 + 总线线
            const int cx = size / 2;
            const int cy = size / 2;
            const int r = qMax(size / 5, 8);

            // CAN 节点圆
            p.setBrush(QColor(QStringLiteral("#173E63")));
            p.setPen(Qt::NoPen);
            p.drawEllipse(QPointF(cx, cy), r, r);

            // 内部 "CAN" 文字
            p.setPen(QPen(QColor(QStringLiteral("#FFFFFF")), qMax(1, size / 20)));
            p.setFont(QFont(QStringLiteral("Arial"), qMax(7, size / 7), QFont::Bold));
            p.drawText(QRectF(cx - r, cy - r, r * 2, r * 2),
                       Qt::AlignCenter, QStringLiteral("CAN"));

            // 总线线
            p.setPen(QPen(QColor(QStringLiteral("#2F7FB5")), qMax(2, size / 18)));
            p.drawLine(QPointF(cx - r - size * 0.12, cy),
                       QPointF(cx + r + size * 0.12, cy));
            p.drawLine(QPointF(cx - r - size * 0.12, cy - size * 0.08),
                       QPointF(cx - r - size * 0.12, cy + size * 0.08));

            // 右端箭头
            p.drawLine(QPointF(cx + r + size * 0.12, cy),
                       QPointF(cx + r + size * 0.08, cy - size * 0.06));
            p.drawLine(QPointF(cx + r + size * 0.12, cy),
                       QPointF(cx + r + size * 0.08, cy + size * 0.06));
        }
        else if (type == QStringLiteral("guide"))
        {
            // 书本图标（用户手册）
            const int lx = static_cast<int>(size * 0.24);
            const int rx = static_cast<int>(size * 0.76);
            const int ty = static_cast<int>(size * 0.16);
            const int by = static_cast<int>(size * 0.84);
            const int spine = static_cast<int>(size * 0.5);

            // 左页
            QPainterPath leftPage;
            leftPage.moveTo(lx, ty);
            leftPage.lineTo(spine, ty);
            leftPage.lineTo(spine, by);
            leftPage.lineTo(lx, by);
            leftPage.closeSubpath();
            p.fillPath(leftPage, QColor(QStringLiteral("#FFFFFF")));
            p.setPen(QPen(QColor(QStringLiteral("#173E63")), qMax(1, size / 28)));
            p.drawPath(leftPage);

            // 右页
            QPainterPath rightPage;
            rightPage.moveTo(spine, ty);
            rightPage.lineTo(rx, ty);
            rightPage.lineTo(rx, by);
            rightPage.lineTo(spine, by);
            rightPage.closeSubpath();
            p.fillPath(rightPage, QColor(QStringLiteral("#F5F9FC")));
            p.drawPath(rightPage);

            // 书脊线
            p.setPen(QPen(QColor(QStringLiteral("#2F7FB5")), qMax(2, size / 24)));
            p.drawLine(QPointF(spine, ty + size * 0.04), QPointF(spine, by));

            // 文字线条（左页 2 条，右页 3 条）
            p.setPen(QPen(QColor(QStringLiteral("#90A4AE")), qMax(1, size / 34)));
            const int lineLeft = lx + size * 0.06;
            const int lineRight = spine + size * 0.06;
            const int lineW1 = static_cast<int>(size * 0.2);
            const int lineW2 = static_cast<int>(size * 0.16);
            const int lineStart = ty + size * 0.22;
            const int lineGap = static_cast<int>(size * 0.16);
            for (int i = 0; i < 3; ++i)
            {
                const int ly = lineStart + i * lineGap;
                if (i < 2)
                    p.drawLine(QPointF(lineLeft, ly), QPointF(lineLeft + lineW1, ly));
                p.drawLine(QPointF(lineRight, ly), QPointF(lineRight + lineW2, ly));
            }

            // 书签
            p.setBrush(QColor(QStringLiteral("#FF9800")));
            p.setPen(Qt::NoPen);
            const int bmW = qMax(4, size / 16);
            const int bmH = static_cast<int>(size * 0.12);
            p.drawRoundedRect(
                QRectF(rx - size * 0.06, ty - size * 0.02, bmW, bmH),
                size * 0.01, size * 0.01);
        }
        else
        {
            const QRectF file(size * 0.28, size * 0.14, size * 0.44, size * 0.68);
            QPainterPath filePath;
            filePath.addRoundedRect(file, size * 0.035, size * 0.035);
            p.fillPath(filePath, QColor(QStringLiteral("#FFFFFF")));
            p.setPen(QPen(QColor(QStringLiteral("#173E63")), qMax(2, size / 26)));
            p.drawPath(filePath);

            p.setBrush(QColor(QStringLiteral("#DCECF8")));
            p.setPen(Qt::NoPen);
            for (int i = 0; i < 3; ++i)
            {
                p.drawRoundedRect(QRectF(size * 0.38, size * (0.34 + i * 0.13), size * 0.24, size * 0.055),
                                  size * 0.01, size * 0.01);
            }
            p.setPen(QPen(QColor(QStringLiteral("#2F7FB5")), qMax(2, size / 24), Qt::SolidLine, Qt::RoundCap));
            p.drawLine(QPointF(size * 0.34, size * 0.78), QPointF(size * 0.66, size * 0.78));
        }

        return pixmap;
    }

    QPixmap makeHomeIconPixmap(int size)
    {
        QPixmap pixmap(size, size);
        pixmap.fill(Qt::transparent);

        QPainter p(&pixmap);
        p.setRenderHint(QPainter::Antialiasing);

        const QRectF house(size * 0.2, size * 0.38, size * 0.6, size * 0.42);
        QPainterPath body;
        body.addRoundedRect(house, size * 0.06, size * 0.06);
        p.fillPath(body, QColor(QStringLiteral("#2F7FB5")));

        QPainterPath roof;
        roof.moveTo(size * 0.16, size * 0.42);
        roof.lineTo(size * 0.5, size * 0.14);
        roof.lineTo(size * 0.84, size * 0.42);
        roof.closeSubpath();
        p.fillPath(roof, QColor(QStringLiteral("#173E63")));

        p.setBrush(QColor(QStringLiteral("#EEF5FB")));
        p.setPen(Qt::NoPen);
        p.drawRoundedRect(QRectF(size * 0.43, size * 0.56, size * 0.14, size * 0.24),
                          size * 0.02, size * 0.02);

        return pixmap;
    }
}
