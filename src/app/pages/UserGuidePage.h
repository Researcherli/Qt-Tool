#ifndef EST_USERGUIDEPAGE_H
#define EST_USERGUIDEPAGE_H

#include <QWidget>

class QTextBrowser;
class QLineEdit;

namespace est
{

    /**
     * 用户手册页面 — 展示所有功能的详细使用说明书。
     *
     * 页面包含目录索引 + 各功能完整文档，支持关键词搜索。
     */
    class UserGuidePage : public QWidget
    {
        Q_OBJECT

    public:
        explicit UserGuidePage(QWidget *parent = nullptr);

    private:
        void setupUi();
        void buildDocument();
        void filterContent(const QString &keyword);

        QLineEdit *m_searchEdit = nullptr;
        QTextBrowser *m_browser = nullptr;
        QString m_fullDocument;
    };

} // namespace est

#endif // EST_USERGUIDEPAGE_H
