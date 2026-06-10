#include "widgets/ProtocolTemplateEditor.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QRegularExpression>
#include <QSpinBox>
#include <QTableWidget>
#include <QVBoxLayout>

namespace est
{
    namespace
    {
        QString normalizedHex(const QString &text)
        {
            QString value = text;
            value.remove(QRegularExpression(QStringLiteral("\\s")));
            return value;
        }

        QComboBox *createTypeCombo(QWidget *parent, FieldType current)
        {
            auto *combo = new QComboBox(parent);
            combo->addItem(QObject::tr("常量"), static_cast<int>(FieldType::Constant));
            combo->addItem(QObject::tr("长度"), static_cast<int>(FieldType::Length));
            combo->addItem(QObject::tr("类型"), static_cast<int>(FieldType::Type));
            combo->addItem(QObject::tr("地址"), static_cast<int>(FieldType::Address));
            combo->addItem(QObject::tr("负载"), static_cast<int>(FieldType::Payload));
            combo->addItem(QObject::tr("校验"), static_cast<int>(FieldType::Checksum));
            combo->addItem(QObject::tr("序号"), static_cast<int>(FieldType::Sequence));
            combo->addItem(QObject::tr("状态"), static_cast<int>(FieldType::Status));
            combo->addItem(QObject::tr("保留"), static_cast<int>(FieldType::Reserved));
            combo->addItem(QObject::tr("自定义"), static_cast<int>(FieldType::Custom));
            const int idx = combo->findData(static_cast<int>(current));
            if (idx >= 0)
                combo->setCurrentIndex(idx);
            return combo;
        }

        QComboBox *createDisplayCombo(QWidget *parent, FieldDisplay current)
        {
            auto *combo = new QComboBox(parent);
            combo->addItem(QStringLiteral("HEX"), static_cast<int>(FieldDisplay::Hex));
            combo->addItem(QStringLiteral("DEC"), static_cast<int>(FieldDisplay::Dec));
            combo->addItem(QObject::tr("有符号"), static_cast<int>(FieldDisplay::Signed));
            combo->addItem(QStringLiteral("BIN"), static_cast<int>(FieldDisplay::Bin));
            combo->addItem(QStringLiteral("ASCII"), static_cast<int>(FieldDisplay::ASCII));
            combo->addItem(QStringLiteral("FLOAT"), static_cast<int>(FieldDisplay::Float));
            const int idx = combo->findData(static_cast<int>(current));
            if (idx >= 0)
                combo->setCurrentIndex(idx);
            return combo;
        }
    }

    ProtocolTemplateEditor::ProtocolTemplateEditor(QWidget *parent)
        : QDialog(parent)
    {
        setWindowTitle(tr("协议模板编辑"));
        setMinimumSize(680, 480);
        setupUi();
    }

    void ProtocolTemplateEditor::setupUi()
    {
        auto *root = new QVBoxLayout(this);

        // --- 基本信息 ---
        auto *form = new QFormLayout();

        m_nameEdit = new QLineEdit(this);
        m_nameEdit->setObjectName(QStringLiteral("protocolTemplateNameEdit"));
        m_nameEdit->setPlaceholderText(tr("如：UART通信协议"));
        form->addRow(tr("模板名称"), m_nameEdit);

        m_descEdit = new QLineEdit(this);
        m_descEdit->setObjectName(QStringLiteral("protocolTemplateDescEdit"));
        m_descEdit->setPlaceholderText(tr("可选的模板说明"));
        form->addRow(tr("描述"), m_descEdit);

        m_endianCombo = new QComboBox(this);
        m_endianCombo->setObjectName(QStringLiteral("protocolTemplateEndianCombo"));
        m_endianCombo->addItem(tr("小端 LE"));
        m_endianCombo->addItem(tr("大端 BE"));
        form->addRow(tr("字节序"), m_endianCombo);

        m_checksumCombo = new QComboBox(this);
        m_checksumCombo->setObjectName(QStringLiteral("protocolTemplateChecksumCombo"));
        m_checksumCombo->addItem(tr("无"), QStringLiteral(""));
        m_checksumCombo->addItem(tr("XOR"), QStringLiteral("xor"));
        m_checksumCombo->addItem(tr("累加和"), QStringLiteral("sum"));
        m_checksumCombo->addItem(tr("CRC8"), QStringLiteral("crc8"));
        form->addRow(tr("校验算法"), m_checksumCombo);

        m_maxFrameSpin = new QSpinBox(this);
        m_maxFrameSpin->setObjectName(QStringLiteral("protocolTemplateMaxFrameSpin"));
        m_maxFrameSpin->setRange(16, 4096);
        m_maxFrameSpin->setValue(256);
        m_maxFrameSpin->setSuffix(tr(" 字节"));
        form->addRow(tr("最大帧长"), m_maxFrameSpin);

        root->addLayout(form);

        // --- 字段列表 ---
        auto *fieldLabel = new QLabel(tr("字段列表（按帧顺序）"), this);
        fieldLabel->setStyleSheet(QStringLiteral("font-weight: bold; margin-top: 8px;"));
        root->addWidget(fieldLabel);

        m_fieldTable = new QTableWidget(this);
        m_fieldTable->setObjectName(QStringLiteral("protocolTemplateFieldTable"));
        m_fieldTable->setColumnCount(7);
        m_fieldTable->setHorizontalHeaderLabels({
            tr("名称"), tr("类型"), tr("偏移"), tr("大小(字节)"),
            tr("期望值(HEX)"), tr("显示格式"), tr("颜色")
        });
        m_fieldTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
        m_fieldTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
        m_fieldTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
        m_fieldTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
        m_fieldTable->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
        m_fieldTable->horizontalHeader()->setSectionResizeMode(5, QHeaderView::ResizeToContents);
        m_fieldTable->horizontalHeader()->setSectionResizeMode(6, QHeaderView::ResizeToContents);
        m_fieldTable->setSelectionBehavior(QAbstractItemView::SelectRows);
        m_fieldTable->setSelectionMode(QAbstractItemView::SingleSelection);
        m_fieldTable->verticalHeader()->setVisible(false);
        root->addWidget(m_fieldTable, 1);

        // --- 字段操作按钮 ---
        auto *btnLayout = new QHBoxLayout();
        auto *addBtn = new QPushButton(tr("添加字段"), this);
        auto *removeBtn = new QPushButton(tr("删除字段"), this);
        auto *upBtn = new QPushButton(tr("↑上移"), this);
        auto *downBtn = new QPushButton(tr("↓下移"), this);
        btnLayout->addWidget(addBtn);
        btnLayout->addWidget(removeBtn);
        btnLayout->addWidget(upBtn);
        btnLayout->addWidget(downBtn);
        btnLayout->addStretch();
        root->addLayout(btnLayout);

        // --- 确定/取消 ---
        auto *buttons = new QDialogButtonBox(
            QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
        root->addWidget(buttons);

        // 连接
        connect(addBtn, &QPushButton::clicked, this, &ProtocolTemplateEditor::onAddField);
        connect(removeBtn, &QPushButton::clicked, this, &ProtocolTemplateEditor::onRemoveField);
        connect(upBtn, &QPushButton::clicked, this, &ProtocolTemplateEditor::onMoveFieldUp);
        connect(downBtn, &QPushButton::clicked, this, &ProtocolTemplateEditor::onMoveFieldDown);
        connect(buttons, &QDialogButtonBox::accepted, this, &ProtocolTemplateEditor::onAccept);
        connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    }

    void ProtocolTemplateEditor::setTemplate(const ProtocolTemplate &tmpl)
    {
        m_editing = true;
        m_nameEdit->setText(tmpl.name);
        m_descEdit->setText(tmpl.description);
        m_endianCombo->setCurrentIndex(
            tmpl.endian == ByteDataInspectorService::Endian::Big ? 1 : 0);
        m_maxFrameSpin->setValue(tmpl.maxFrameSize);

        for (int i = 0; i < m_checksumCombo->count(); ++i)
        {
            if (m_checksumCombo->itemData(i).toString() == tmpl.checksumAlgo)
            {
                m_checksumCombo->setCurrentIndex(i);
                break;
            }
        }

        m_fields = tmpl.fields;
        refreshFieldTable();
    }

    ProtocolTemplate ProtocolTemplateEditor::getTemplate() const
    {
        ProtocolTemplate tmpl;
        tmpl.name = m_nameEdit->text().trimmed();
        tmpl.description = m_descEdit->text().trimmed();
        tmpl.endian = (m_endianCombo->currentIndex() == 1)
                          ? ByteDataInspectorService::Endian::Big
                          : ByteDataInspectorService::Endian::Little;
        tmpl.maxFrameSize = m_maxFrameSpin->value();
        tmpl.checksumAlgo = m_checksumCombo->currentData().toString();
        tmpl.fields = m_fields;
        return tmpl;
    }

    void ProtocolTemplateEditor::refreshFieldTable()
    {
        m_fieldTable->setRowCount(m_fields.size());

        for (int row = 0; row < m_fields.size(); ++row)
        {
            const auto &f = m_fields[row];

            auto *nameItem = new QTableWidgetItem(f.name);
            m_fieldTable->setItem(row, 0, nameItem);

            m_fieldTable->setCellWidget(row, 1, createTypeCombo(m_fieldTable, f.type));

            auto *offsetItem = new QTableWidgetItem(QString::number(f.offset));
            m_fieldTable->setItem(row, 2, offsetItem);

            auto *sizeItem = new QTableWidgetItem(
                f.size > 0 ? QString::number(f.size) : QStringLiteral("自动"));
            m_fieldTable->setItem(row, 3, sizeItem);

            auto *expectItem = new QTableWidgetItem(
                QString::fromLatin1(f.expectedValue.toHex(' ').toUpper()));
            m_fieldTable->setItem(row, 4, expectItem);

            m_fieldTable->setCellWidget(row, 5, createDisplayCombo(m_fieldTable, f.display));

            auto *colorItem = new QTableWidgetItem(f.highlightColor.name());
            colorItem->setBackground(f.highlightColor);
            m_fieldTable->setItem(row, 6, colorItem);
        }
    }

    void ProtocolTemplateEditor::readFieldsFromTable()
    {
        m_fields.clear();
        for (int row = 0; row < m_fieldTable->rowCount(); ++row)
        {
            FieldDef f;
            f.name = m_fieldTable->item(row, 0) ? m_fieldTable->item(row, 0)->text().trimmed() : QString();
            if (auto *typeCombo = qobject_cast<QComboBox *>(m_fieldTable->cellWidget(row, 1)))
                f.type = static_cast<FieldType>(typeCombo->currentData().toInt());
            else
                f.type = FieldType::Custom;
            f.offset = m_fieldTable->item(row, 2) ? m_fieldTable->item(row, 2)->text().toInt() : 0;

            const QString sizeText = m_fieldTable->item(row, 3) ? m_fieldTable->item(row, 3)->text() : QStringLiteral("1");
            f.size = (sizeText == QStringLiteral("自动")) ? 0 : sizeText.toInt();

            const QString expectedHex = m_fieldTable->item(row, 4)
                                            ? normalizedHex(m_fieldTable->item(row, 4)->text())
                                            : QString();
            f.expectedValue = QByteArray::fromHex(expectedHex.toLatin1());

            if (auto *displayCombo = qobject_cast<QComboBox *>(m_fieldTable->cellWidget(row, 5)))
                f.display = static_cast<FieldDisplay>(displayCombo->currentData().toInt());
            else
                f.display = FieldDisplay::Hex;

            if (m_fieldTable->item(row, 6))
                f.highlightColor = m_fieldTable->item(row, 6)->background().color();

            m_fields.append(f);
        }
    }

    void ProtocolTemplateEditor::onAddField()
    {
        readFieldsFromTable();
        FieldDef f;
        f.name = tr("字段%1").arg(m_fields.size() + 1);
        f.offset = m_fields.isEmpty() ? 0 : (m_fields.last().offset + (m_fields.last().size > 0 ? m_fields.last().size : 1));
        m_fields.append(f);
        refreshFieldTable();
    }

    void ProtocolTemplateEditor::onRemoveField()
    {
        int row = m_fieldTable->currentRow();
        if (row < 0 || row >= m_fields.size())
            return;
        readFieldsFromTable();
        m_fields.removeAt(row);
        refreshFieldTable();
    }

    void ProtocolTemplateEditor::onMoveFieldUp()
    {
        int row = m_fieldTable->currentRow();
        if (row <= 0 || row >= m_fields.size())
            return;
        readFieldsFromTable();
        m_fields.swapItemsAt(row, row - 1);
        refreshFieldTable();
        m_fieldTable->setCurrentCell(row - 1, 0);
    }

    void ProtocolTemplateEditor::onMoveFieldDown()
    {
        int row = m_fieldTable->currentRow();
        if (row < 0 || row >= m_fields.size() - 1)
            return;
        readFieldsFromTable();
        m_fields.swapItemsAt(row, row + 1);
        refreshFieldTable();
        m_fieldTable->setCurrentCell(row + 1, 0);
    }

    void ProtocolTemplateEditor::onAccept()
    {
        readFieldsFromTable();

        if (m_nameEdit->text().trimmed().isEmpty())
        {
            QMessageBox::warning(this, tr("提示"), tr("请输入模板名称"));
            return;
        }

        if (m_fields.isEmpty())
        {
            QMessageBox::warning(this, tr("提示"), tr("请至少添加一个字段"));
            return;
        }

        int checksumCount = 0;
        const QRegularExpression hexPattern(QStringLiteral("^[0-9A-Fa-f]*$"));
        for (int row = 0; row < m_fields.size(); ++row)
        {
            const FieldDef &field = m_fields[row];
            const QString rowLabel = tr("第 %1 行").arg(row + 1);
            if (field.name.isEmpty())
            {
                QMessageBox::warning(this, tr("提示"), tr("%1：字段名称不能为空").arg(rowLabel));
                return;
            }
            if (field.offset < 0 && field.type != FieldType::Checksum)
            {
                QMessageBox::warning(this, tr("提示"), tr("%1：只有校验字段允许使用 -1 表示帧尾").arg(rowLabel));
                return;
            }
            if (field.size < 0)
            {
                QMessageBox::warning(this, tr("提示"), tr("%1：字段大小不能为负数").arg(rowLabel));
                return;
            }
            if (field.size == 0 && field.type != FieldType::Payload)
            {
                QMessageBox::warning(this, tr("提示"), tr("%1：只有负载字段支持自动大小").arg(rowLabel));
                return;
            }

            const QString expectedText = m_fieldTable->item(row, 4)
                                             ? normalizedHex(m_fieldTable->item(row, 4)->text())
                                             : QString();
            if (!expectedText.isEmpty()
                && (!hexPattern.match(expectedText).hasMatch() || expectedText.size() % 2 != 0))
            {
                QMessageBox::warning(this, tr("提示"), tr("%1：期望值必须是完整的 HEX 字节").arg(rowLabel));
                return;
            }
            if (field.type == FieldType::Constant
                && !field.expectedValue.isEmpty()
                && field.size > 0
                && field.expectedValue.size() != field.size)
            {
                QMessageBox::warning(this, tr("提示"), tr("%1：常量期望值长度需要和字段大小一致").arg(rowLabel));
                return;
            }
            if (field.type == FieldType::Checksum)
                ++checksumCount;
        }

        if (checksumCount > 1)
        {
            QMessageBox::warning(this, tr("提示"), tr("一个模板最多只能包含一个校验字段"));
            return;
        }

        accept();
    }

} // namespace est
