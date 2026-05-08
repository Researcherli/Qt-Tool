#pragma once

#include <QAbstractScrollArea>
#include <QList>
#include <QFont>

#include "FileCompareEngine.h"

namespace est {

class DiffView : public QAbstractScrollArea
{
    Q_OBJECT

public:
    explicit DiffView(QWidget *parent = nullptr);

    void setDiffData(const QList<DiffLine> &diffs);
    void setLineNumbersVisible(bool visible);
    void goToLine(int lineIndex);
    void setViewFont(const QFont &font);

signals:
    void lineClicked(int lineIndex);

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;

private:
    void updateScrollBars();
    int lineHeight() const;

    QList<DiffLine> m_diffs;
    QFont m_font;
    bool m_showLineNumbers = true;
    int m_gutterWidth = 40;
    int m_lineSpacing = 2;

    // 颜色
    QColor m_bgColor{37, 37, 38};
    QColor m_gutterBgColor{30, 30, 30};
    QColor m_gutterTextColor{133, 133, 133};
    QColor m_addedBgColor{76, 175, 80, 40};
    QColor m_removedBgColor{229, 115, 115, 40};
    QColor m_modifiedBgColor{255, 183, 77, 40};
    QColor m_textColor{212, 212, 212};
    QColor m_separatorColor{64, 64, 64};
};

} // namespace est
