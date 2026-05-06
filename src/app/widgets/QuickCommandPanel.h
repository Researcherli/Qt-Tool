#ifndef EST_QUICKCOMMANDPANEL_H
#define EST_QUICKCOMMANDPANEL_H

#include <QList>
#include <QWidget>

class QListWidget;

namespace est
{

    struct QuickCommandDefinition
    {
        QString name;
        QString content;
        QString mode;
        bool appendNewLine = true;
        QString lineEnding = QStringLiteral("crlf");
        QString remark;
    };

    class QuickCommandPanel : public QWidget
    {
        Q_OBJECT

    public:
        explicit QuickCommandPanel(QWidget *parent = nullptr);

        void promptAddCommand(const QuickCommandDefinition &draft);

    signals:
        void commandTriggered(const est::QuickCommandDefinition &command);
        void statusMessageGenerated(const QString &text);

    private:
        void loadCommands();
        void saveCommands() const;
        void rebuildList();
        void editCommand(int index);
        QString storagePath() const;

        QList<QuickCommandDefinition> m_commands;
        QListWidget *m_listWidget = nullptr;
    };

} // namespace est

Q_DECLARE_METATYPE(est::QuickCommandDefinition)

#endif // EST_QUICKCOMMANDPANEL_H
