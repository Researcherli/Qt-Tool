#include "DiffView.h"

#include <QPainter>
#include <QPaintEvent>
#include <QResizeEvent>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QScrollBar>
#include <QFontMetrics>

namespace est {

DiffView::DiffView(QWidget *parent)
    : QAbstractScrollArea(parent)
{
    m_font = QFont(QStringLiteral("JetBrains Mono"), 10);
    m_font.setStyleHint(QFont::Monospace);
    viewport()->setAutoFillBackground(false);
    setMouseTracking(true);
}

void DiffView::setDiffData(const QList<DiffLine> &diffs)
{
    m_diffs = diffs;
    updateScrollBars();
    viewport()->update();
}

void DiffView::setLineNumbersVisible(bool visible)
{
    m_showLineNumbers = visible;
    if (!visible) m_gutterWidth = 0;
    else m_gutterWidth = 40;
    updateScrollBars();
    viewport()->update();
}

void DiffView::goToLine(int lineIndex)
{
    if (lineIndex < 0 || lineIndex >= m_diffs.size()) return;
    int y = lineIndex * lineHeight();
    verticalScrollBar()->setValue(y);
}

void DiffView::setViewFont(const QFont &font)
{
    m_font = font;
    updateScrollBars();
    viewport()->update();
}

int DiffView::lineHeight() const
{
    return QFontMetrics(m_font).lineSpacing() + m_lineSpacing;
}

void DiffView::updateScrollBars()
{
    int totalHeight = m_diffs.size() * lineHeight();
    int viewHeight = viewport()->height();
    verticalScrollBar()->setRange(0, std::max(0, totalHeight - viewHeight));
    verticalScrollBar()->setSingleStep(lineHeight());
    verticalScrollBar()->setPageStep(viewHeight);
}

void DiffView::paintEvent(QPaintEvent *event)
{
    QPainter painter(viewport());
    painter.fillRect(event->rect(), m_bgColor);

    if (m_diffs.isEmpty()) return;

    painter.setFont(m_font);
    int lh = lineHeight();
    int scrollY = verticalScrollBar()->value();
    int viewW = viewport()->width();
    int viewH = viewport()->height();

    int firstVisible = scrollY / lh;
    int lastVisible = std::min(static_cast<int>(m_diffs.size()) - 1,
                                (scrollY + viewH) / lh);

    int leftWidth = (viewW - m_gutterWidth) / 2;
    int rightX = m_gutterWidth + leftWidth;

    for (int i = firstVisible; i <= lastVisible; ++i) {
        int y = i * lh - scrollY;
        const DiffLine &diff = m_diffs.at(i);

        // 行背景色
        QColor bgColor;
        switch (diff.status) {
        case DiffLine::Added:    bgColor = m_addedBgColor; break;
        case DiffLine::Removed:  bgColor = m_removedBgColor; break;
        case DiffLine::Modified: bgColor = m_modifiedBgColor; break;
        default: break;
        }
        if (bgColor.isValid()) {
            painter.fillRect(m_gutterWidth, y, viewW - m_gutterWidth, lh, bgColor);
        }

        // 行号
        if (m_showLineNumbers) {
            painter.fillRect(0, y, m_gutterWidth, lh, m_gutterBgColor);
            painter.setPen(m_gutterTextColor);
            painter.drawText(2, y, m_gutterWidth - 4, lh,
                             Qt::AlignRight | Qt::AlignVCenter,
                             QString::number(i + 1));
        }

        // 分隔线
        painter.setPen(m_separatorColor);
        painter.drawLine(rightX, y, rightX, y + lh);

        // 左侧文本
        painter.setPen(m_textColor);
        QString leftText = diff.textA.isEmpty() ? QString() : diff.textA;
        painter.drawText(m_gutterWidth + 4, y, leftWidth - 8, lh,
                         Qt::AlignLeft | Qt::AlignVCenter,
                         leftText);

        // 右侧文本
        QString rightText = diff.textB.isEmpty() ? QString() : diff.textB;
        painter.drawText(rightX + 4, y, leftWidth - 8, lh,
                         Qt::AlignLeft | Qt::AlignVCenter,
                         rightText);
    }
}

void DiffView::resizeEvent(QResizeEvent *event)
{
    QAbstractScrollArea::resizeEvent(event);
    updateScrollBars();
}

void DiffView::mousePressEvent(QMouseEvent *event)
{
    if (m_diffs.isEmpty()) return;
    int lh = lineHeight();
    int scrollY = verticalScrollBar()->value();
    int lineIndex = (event->pos().y() + scrollY) / lh;
    if (lineIndex >= 0 && lineIndex < m_diffs.size()) {
        emit lineClicked(lineIndex);
    }
}

void DiffView::wheelEvent(QWheelEvent *event)
{
    QAbstractScrollArea::wheelEvent(event);
}

} // namespace est
