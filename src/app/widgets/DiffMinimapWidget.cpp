#include "DiffMinimapWidget.h"
#include <QPainter>
#include <QMouseEvent>
#include <QResizeEvent>

namespace est {

DiffMinimapWidget::DiffMinimapWidget(QWidget* parent)
    : QWidget(parent)
{
    setMinimumWidth(30);
    setMaximumWidth(50);
}

void DiffMinimapWidget::setDiffData(const QList<DiffLine>& data)
{
    m_diffData = data;
    m_cache = QPixmap();
    if (!m_diffData.isEmpty()) {
        rebuildCache();
    }
    update();
}

void DiffMinimapWidget::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);

    QPainter painter(this);
    painter.fillRect(rect(), m_colorBackground);

    if (!m_cache.isNull()) {
        painter.drawPixmap(0, 0, m_cache);
        return;
    }
}

void DiffMinimapWidget::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    m_cache = QPixmap();
    if (!m_diffData.isEmpty()) {
        rebuildCache();
    }
}

void DiffMinimapWidget::rebuildCache()
{
    m_cache = QPixmap(size());
    m_cache.fill(m_colorBackground);

    QPainter painter(&m_cache);
    painter.setPen(Qt::NoPen);

    int totalLines = m_diffData.size();
    if (totalLines == 0) return;

    double hRatio = static_cast<double>(height()) / totalLines;

    // When each line has enough vertical space (hRatio >= 1.5),
    // use the original run-batching approach for performance.
    // When space is tight, switch to pixel-level diff-priority drawing
    // so that diff lines (Added/Removed/Modified) are always visible.
    if (hRatio >= 1.5) {
        int i = 0;
        while (i < totalLines) {
            DiffLine::Status currentStatus = m_diffData[i].status;
            int runStart = i;
            while (i < totalLines && m_diffData[i].status == currentStatus) ++i;

            QColor color = colorForStatus(currentStatus);
            double y = runStart * hRatio;
            double h = (i - runStart) * hRatio;
            if (h < 1.0) h = 1.0;

            painter.fillRect(QRectF(0, y, width(), h), color);
        }
    } else {
        // Pixel-level diff-priority rendering.
        // For each pixel row on the minimap, scan the corresponding
        // source lines [startLine, endLine). If any is a diff line,
        // draw that pixel row in the diff colour; otherwise draw Equal.
        for (int py = 0; py < height(); ++py) {
            int startLine = static_cast<int>(py / hRatio);
            int endLine   = static_cast<int>((py + 1) / hRatio);
            if (startLine >= totalLines) break;

            DiffLine::Status status = DiffLine::Equal;
            for (int line = startLine; line < endLine && line < totalLines; ++line) {
                if (m_diffData[line].status != DiffLine::Equal) {
                    status = m_diffData[line].status;
                    break; // diff-priority: first diff status wins
                }
            }
            painter.fillRect(QRectF(0, py, width(), 1), colorForStatus(status));
        }
    }
}

QColor DiffMinimapWidget::colorForStatus(DiffLine::Status status) const
{
    switch (status) {
        case DiffLine::Added:    return m_colorAdded;
        case DiffLine::Removed:  return m_colorRemoved;
        case DiffLine::Modified: return m_colorModified;
        case DiffLine::Equal:    return m_colorEqual;
    }
    return m_colorEqual;
}

void DiffMinimapWidget::mousePressEvent(QMouseEvent* event)
{
    if (event->button() != Qt::LeftButton) {
        QWidget::mousePressEvent(event);
        return;
    }
    if (m_diffData.isEmpty()) {
        QWidget::mousePressEvent(event);
        return;
    }

    m_isDragging = true;
    setMouseTracking(true);

    int totalLines = m_diffData.size();
    double hRatio = static_cast<double>(height()) / totalLines;
    int lineIndex = qBound(0, static_cast<int>(event->pos().y() / hRatio), totalLines - 1);

    emit lineClicked(lineIndex);
}

void DiffMinimapWidget::mouseMoveEvent(QMouseEvent* event)
{
    if (!m_isDragging || m_diffData.isEmpty()) {
        QWidget::mouseMoveEvent(event);
        return;
    }

    int totalLines = m_diffData.size();
    double hRatio = static_cast<double>(height()) / totalLines;
    int lineIndex = qBound(0, static_cast<int>(event->pos().y() / hRatio), totalLines - 1);

    emit lineClicked(lineIndex);
}

void DiffMinimapWidget::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() != Qt::LeftButton || !m_isDragging) {
        QWidget::mouseReleaseEvent(event);
        return;
    }

    m_isDragging = false;
    setMouseTracking(false);

    // Also emit a final jump on release so the target is hit precisely
    if (!m_diffData.isEmpty()) {
        int totalLines = m_diffData.size();
        double hRatio = static_cast<double>(height()) / totalLines;
        int lineIndex = qBound(0, static_cast<int>(event->pos().y() / hRatio), totalLines - 1);
        emit lineClicked(lineIndex);
    }
}

} // namespace est
