#pragma once

#include <QWidget>
#include <QList>
#include <QPixmap>
#include <QColor>
#include "FileCompareEngine.h"

namespace est {

class DiffMinimapWidget : public QWidget {
    Q_OBJECT

public:
    explicit DiffMinimapWidget(QWidget* parent = nullptr);

    void setDiffData(const QList<DiffLine>& data);

signals:
    void lineClicked(int lineIndex);

protected:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;

private:
    void rebuildCache();
    QColor colorForStatus(DiffLine::Status status) const;

    QList<DiffLine> m_diffData;
    QPixmap m_cache;
    bool m_isDragging = false;

    QColor m_colorAdded     = QColor("#81C784");  // Soft green, light theme
    QColor m_colorRemoved   = QColor("#E57373");  // Soft red
    QColor m_colorModified  = QColor("#FFB74D");  // Soft orange
    QColor m_colorEqual     = QColor("#D6E3EF");  // Light border, blends with light theme
    QColor m_colorBackground= QColor("#EEF5FB");  // Match page background
};

} // namespace est
