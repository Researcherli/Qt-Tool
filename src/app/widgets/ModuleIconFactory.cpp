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
