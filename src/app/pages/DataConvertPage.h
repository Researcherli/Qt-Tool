#ifndef EST_DATACONVERTPAGE_H
#define EST_DATACONVERTPAGE_H

#include <QWidget>

namespace est
{

    class DataConvertWidget;

    class DataConvertPage : public QWidget
    {
        Q_OBJECT

    public:
        explicit DataConvertPage(QWidget *parent = nullptr);

    signals:
        void statusMessageGenerated(const QString &text);

    private:
        DataConvertWidget *m_dataConvertWidget = nullptr;
    };

} // namespace est

#endif // EST_DATACONVERTPAGE_H
