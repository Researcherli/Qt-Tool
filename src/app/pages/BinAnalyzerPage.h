#ifndef EST_BINANALYZERPAGE_H
#define EST_BINANALYZERPAGE_H

#include <QWidget>

namespace est
{

    class BinAnalyzerWidget;
    class ICore;

    class BinAnalyzerPage : public QWidget
    {
        Q_OBJECT

    public:
        explicit BinAnalyzerPage(ICore *core, QWidget *parent = nullptr);

        void openFileDialog();

    signals:
        void currentFileChanged(const QString &fileText);
        void statusMessageGenerated(const QString &text);
        void recentRecordsChanged();

    private:
        BinAnalyzerWidget *m_binAnalyzerWidget = nullptr;
    };

} // namespace est

#endif // EST_BINANALYZERPAGE_H
