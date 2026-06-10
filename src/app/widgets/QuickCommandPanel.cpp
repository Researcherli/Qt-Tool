#include "widgets/QuickCommandPanel.h"

#include "services/AppPaths.h"

#include <QDialog>
#include <QDialogButtonBox>
#include <QFile>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QVBoxLayout>

namespace est
{

    namespace
    {

        QJsonObject toJson(const QuickCommandDefinition &command)
        {
            QJsonObject object;
            object.insert(QStringLiteral("name"), command.name);
            object.insert(QStringLiteral("content"), command.content);
            object.insert(QStringLiteral("mode"), command.mode);
            object.insert(QStringLiteral("appendNewLine"), command.appendNewLine);
            object.insert(QStringLiteral("lineEnding"), command.lineEnding);
            object.insert(QStringLiteral("remark"), command.remark);
            return object;
        }

        QuickCommandDefinition fromJson(const QJsonObject &object)
        {
            QuickCommandDefinition command;
            command.name = object.value(QStringLiteral("name")).toString();
            command.content = object.value(QStringLiteral("content")).toString();
            command.mode = object.value(QStringLiteral("mode")).toString(QStringLiteral("text"));
            command.appendNewLine = object.value(QStringLiteral("appendNewLine")).toBool(true);
            command.lineEnding = object.value(QStringLiteral("lineEnding")).toString(QStringLiteral("crlf"));
            command.remark = object.value(QStringLiteral("remark")).toString();
            return command;
        }

        bool openEditorDialog(QWidget *parent,
                              QuickCommandDefinition *command,
                              const QString &title)
        {
            QDialog dialog(parent);
            dialog.setWindowTitle(title);

            auto *layout = new QVBoxLayout(&dialog);
            auto *formLayout = new QFormLayout();

            auto *nameEdit = new QLineEdit(command->name, &dialog);
            auto *contentEdit = new QPlainTextEdit(command->content, &dialog);
            contentEdit->setFixedHeight(100);

            formLayout->addRow(QObject::tr("命令名称"), nameEdit);
            formLayout->addRow(QObject::tr("命令内容"), contentEdit);

            auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
            layout->addLayout(formLayout);
            layout->addWidget(buttons);

            QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
            QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

            if (dialog.exec() != QDialog::Accepted)
            {
                return false;
            }

            command->name = nameEdit->text().trimmed();
            command->content = contentEdit->toPlainText();
            if (command->name.isEmpty())
            {
                command->name = QObject::tr("未命名命令");
            }
            command->mode = command->mode.trimmed().toLower();
            if (command->mode != QStringLiteral("hex"))
            {
                command->mode = QStringLiteral("text");
            }
            command->lineEnding = command->lineEnding.trimmed().toLower();
            if (command->lineEnding.isEmpty())
            {
                command->lineEnding = QStringLiteral("crlf");
            }
            return true;
        }

    } // namespace

    QuickCommandPanel::QuickCommandPanel(QWidget *parent)
        : QWidget(parent)
    {
        setObjectName(QStringLiteral("quickCommandPanel"));

        auto *rootLayout = new QVBoxLayout(this);
        rootLayout->setContentsMargins(0, 0, 0, 0);
        rootLayout->setSpacing(6);

        auto *titleLabel = new QLabel(tr("快捷命令"), this);
        titleLabel->setObjectName(QStringLiteral("sectionTitle"));
        titleLabel->setMinimumHeight(24);

        m_listWidget = new QListWidget(this);
        m_listWidget->setSelectionMode(QAbstractItemView::SingleSelection);
        // Increase list font size to match send input (20px)
        QFont listFont = m_listWidget->font();
        listFont.setPixelSize(20);
        m_listWidget->setFont(listFont);
        m_listWidget->setStyleSheet(QStringLiteral("font-size:20px;"));

        auto *buttonLayout = new QHBoxLayout();
        buttonLayout->setSpacing(6);
        auto *addButton = new QPushButton(tr("新增"), this);
        auto *editButton = new QPushButton(tr("编辑"), this);
        auto *deleteButton = new QPushButton(tr("删除"), this);

        buttonLayout->addWidget(addButton);
        buttonLayout->addWidget(editButton);
        buttonLayout->addWidget(deleteButton);

        rootLayout->addWidget(titleLabel);
        rootLayout->addWidget(m_listWidget, 1);
        rootLayout->addLayout(buttonLayout);

        loadCommands();
        rebuildList();

        connect(addButton, &QPushButton::clicked, this, [this]() {
            QuickCommandDefinition command;
            if (openEditorDialog(this, &command, tr("新增快捷命令")))
            {
                m_commands.prepend(command);
                saveCommands();
                rebuildList();
                emit statusMessageGenerated(tr("已新增快捷命令：%1").arg(command.name));
            }
        });
        connect(editButton, &QPushButton::clicked, this, [this]() {
            const QListWidgetItem *item = m_listWidget->currentItem();
            if (item == nullptr)
            {
                return;
            }
            editCommand(item->data(Qt::UserRole).toInt());
        });
        connect(deleteButton, &QPushButton::clicked, this, [this]() {
            const QListWidgetItem *item = m_listWidget->currentItem();
            if (item == nullptr)
            {
                return;
            }
            const int index = item->data(Qt::UserRole).toInt();
            if (index < 0 || index >= m_commands.size())
            {
                return;
            }
            const QString name = m_commands.at(index).name;
            m_commands.removeAt(index);
            saveCommands();
            rebuildList();
            emit statusMessageGenerated(tr("已删除快捷命令：%1").arg(name));
        });
        connect(m_listWidget, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem *item) {
            if (item == nullptr)
            {
                return;
            }
            const int index = item->data(Qt::UserRole).toInt();
            if (index >= 0 && index < m_commands.size())
            {
                emit commandTriggered(m_commands.at(index));
            }
        });
    }

    void QuickCommandPanel::promptAddCommand(const QuickCommandDefinition &draft)
    {
        QuickCommandDefinition command = draft;
        if (openEditorDialog(this, &command, tr("保存为快捷命令")))
        {
            m_commands.prepend(command);
            saveCommands();
            rebuildList();
            emit statusMessageGenerated(tr("已保存快捷命令：%1").arg(command.name));
        }
    }

    void QuickCommandPanel::loadCommands()
    {
        QFile file(storagePath());
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        {
            return;
        }

        const QJsonDocument document = QJsonDocument::fromJson(file.readAll());
        const QJsonArray array = document.object().value(QStringLiteral("commands")).toArray();
        for (const QJsonValue &value : array)
        {
            m_commands.append(fromJson(value.toObject()));
        }
    }

    void QuickCommandPanel::saveCommands() const
    {
        QJsonArray array;
        for (const QuickCommandDefinition &command : m_commands)
        {
            array.append(toJson(command));
        }
        QJsonObject root;
        root.insert(QStringLiteral("commands"), array);

        QFile file(storagePath());
        if (file.open(QIODevice::WriteOnly | QIODevice::Text))
        {
            file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
        }
    }

    void QuickCommandPanel::rebuildList()
    {
        m_listWidget->clear();
        for (int index = 0; index < m_commands.size(); ++index)
        {
            const QuickCommandDefinition &command = m_commands.at(index);
            auto *item = new QListWidgetItem(m_listWidget);
            item->setText(QStringLiteral("%1|%2").arg(command.name, command.content));
            item->setData(Qt::UserRole, index);
            item->setSizeHint(QSize(item->sizeHint().width(), 32));
        }

        if (m_listWidget->count() == 0)
        {
            auto *placeholder = new QListWidgetItem(tr("暂无快捷命令，可点击“新增”或从发送区保存。"), m_listWidget);
            placeholder->setFlags(Qt::NoItemFlags);
            placeholder->setForeground(QBrush(QColor(QStringLiteral("#5F7890"))));
        }
    }

    void QuickCommandPanel::editCommand(int index)
    {
        if (index < 0 || index >= m_commands.size())
        {
            return;
        }

        QuickCommandDefinition command = m_commands.at(index);
        if (openEditorDialog(this, &command, tr("编辑快捷命令")))
        {
            m_commands[index] = command;
            saveCommands();
            rebuildList();
            emit statusMessageGenerated(tr("已更新快捷命令：%1").arg(command.name));
        }
    }

    QString QuickCommandPanel::storagePath() const
    {
        return AppPaths::configFilePath(QStringLiteral("quick_commands.json"));
    }

} // namespace est
