#ifndef EST_INDUSTRIALSTATUSCARD_H
#define EST_INDUSTRIALSTATUSCARD_H

#include <QColor>
#include <QWidget>

class QLabel;
class QFrame;

namespace est
{

    class IndustrialStatusCard : public QWidget
    {
        Q_OBJECT

    public:
        explicit IndustrialStatusCard(const QString &title, QWidget *parent = nullptr);

        void setStatus(const QString &text, const QColor &color);
        void setDetail(const QString &text);

    private:
        QFrame *m_indicator = nullptr;
        QLabel *m_titleLabel = nullptr;
        QLabel *m_statusLabel = nullptr;
        QLabel *m_detailLabel = nullptr;
    };

} // namespace est

#endif // EST_INDUSTRIALSTATUSCARD_H
