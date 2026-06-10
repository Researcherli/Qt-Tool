#include "pages/UserGuidePage.h"

#include <QFile>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QTextBrowser>
#include <QVBoxLayout>

namespace est
{

    UserGuidePage::UserGuidePage(QWidget *parent)
        : QWidget(parent)
    {
        setObjectName(QStringLiteral("userGuidePage"));

        auto *rootLayout = new QVBoxLayout(this);
        rootLayout->setContentsMargins(0, 0, 0, 0);
        rootLayout->setSpacing(0);

        setupUi();
        buildDocument();
    }

    void UserGuidePage::setupUi()
    {
        // 工具栏
        auto *toolbar = new QWidget(this);
        toolbar->setObjectName(QStringLiteral("guideToolbar"));
        auto *toolbarLayout = new QHBoxLayout(toolbar);
        toolbarLayout->setContentsMargins(16, 10, 16, 10);
        toolbarLayout->setSpacing(12);

        auto *iconLabel = new QLabel(QStringLiteral("📖"), toolbar);
        iconLabel->setStyleSheet(QStringLiteral("font-size: 20px;"));
        toolbarLayout->addWidget(iconLabel);

        auto *titleLabel = new QLabel(tr("用户手册"), toolbar);
        titleLabel->setObjectName(QStringLiteral("guideTitle"));
        titleLabel->setStyleSheet(
            QStringLiteral("font-size: 16px; font-weight: bold; color: #333;"));
        toolbarLayout->addWidget(titleLabel);

        toolbarLayout->addSpacing(16);

        m_searchEdit = new QLineEdit(toolbar);
        m_searchEdit->setObjectName(QStringLiteral("guideSearchEdit"));
        m_searchEdit->setPlaceholderText(tr("搜索功能…"));
        m_searchEdit->setClearButtonEnabled(true);
        m_searchEdit->setFixedWidth(280);
        m_searchEdit->setStyleSheet(
            QStringLiteral("QLineEdit {"
                           "  padding: 6px 12px;"
                           "  border: 1px solid #CCC;"
                           "  border-radius: 6px;"
                           "  font-size: 13px;"
                           "}"
                           "QLineEdit:focus {"
                           "  border-color: #2F7FB5;"
                           "}"));
        toolbarLayout->addWidget(m_searchEdit);

        toolbarLayout->addStretch(1);

        auto *rootLayout = qobject_cast<QVBoxLayout *>(layout());
        if (rootLayout)
            rootLayout->addWidget(toolbar);

        // 浏览器
        m_browser = new QTextBrowser(this);
        m_browser->setObjectName(QStringLiteral("guideBrowser"));
        m_browser->setOpenExternalLinks(true);
        m_browser->setOpenLinks(true);
        m_browser->setStyleSheet(
            QStringLiteral("QTextBrowser {"
                           "  background: #FAFAFA;"
                           "  border: none;"
                           "  padding: 20px 40px;"
                           "  font-size: 14px;"
                           "  color: #333;"
                           "}"
                           "QTextBrowser:focus { border: none; }"));

        // 搜索功能
        connect(m_searchEdit, &QLineEdit::textChanged,
                this, &UserGuidePage::filterContent);

        if (rootLayout)
            rootLayout->addWidget(m_browser, 1);
    }

    void UserGuidePage::buildDocument()
    {
        QFile file(QStringLiteral(":/docs/user_guide.html"));
        if (file.open(QIODevice::ReadOnly | QIODevice::Text))
        {
            m_fullDocument = QString::fromUtf8(file.readAll());
            file.close();
        }
        else
        {
            m_fullDocument = QStringLiteral("<h1>%1</h1>").arg(tr("无法加载用户手册"));
        }

        m_browser->setHtml(m_fullDocument);
    }

    void UserGuidePage::filterContent(const QString &keyword)
    {
        if (keyword.isEmpty())
        {
            m_browser->setHtml(m_fullDocument);
            return;
        }

        // 简单高亮：包装匹配文本
        QString html = m_fullDocument;
        const QString escaped = keyword.toHtmlEscaped();
        html.replace(escaped,
                     QStringLiteral("<span style='background: #FFF176; padding: 1px 4px; "
                                    "border-radius: 3px; font-weight: bold;'>%1</span>")
                         .arg(escaped),
                     Qt::CaseInsensitive);

        m_browser->setHtml(html);

        // 尝试跳转到第一个匹配的章节
        const QString anchor = QStringLiteral("id='");
        int pos = html.indexOf(anchor, 0, Qt::CaseInsensitive);
        if (pos >= 0)
        {
            int start = pos + anchor.length();
            int end = html.indexOf(QChar('\''), start);
            if (end > start)
            {
                const QString id = html.mid(start, end - start);
                m_browser->scrollToAnchor(id);
            }
        }
    }

} // namespace est
