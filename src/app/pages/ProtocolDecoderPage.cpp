#include "pages/ProtocolDecoderPage.h"

#include "databus/DataBus.h"
#include "databus/DataPacket.h"
#include "plugin/ICore.h"
#include "widgets/ProtocolTemplateEditor.h"

#include <QComboBox>
#include <QDateTime>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QSplitter>
#include <QTableWidget>
#include <QToolButton>
#include <QVBoxLayout>

namespace est
{

    ProtocolDecoderPage::ProtocolDecoderPage(ICore *core, QWidget *parent)
        : QWidget(parent)
        , m_dataBus(core ? core->dataBus() : nullptr)
        , m_decoder(new ProtocolDecoder(this))
    {
        setObjectName(QStringLiteral("protocolDecoderPage"));

        auto *rootLayout = new QVBoxLayout(this);
        rootLayout->setContentsMargins(8, 8, 8, 8);
        rootLayout->setSpacing(8);

        setupUi();

        // 从 QSettings 加载模板
        onLoadTemplates();

        subscribeDataBus();
    }

    ProtocolDecoderPage::~ProtocolDecoderPage()
    {
        if (m_subscription.isValid() && m_dataBus)
            m_dataBus->unsubscribe(m_subscription);
    }

    void ProtocolDecoderPage::setupUi()
    {
        // --- 工具栏 ---
        auto *toolbar = new QWidget(this);
        toolbar->setObjectName(QStringLiteral("protoToolbar"));
        auto *tbLayout = new QHBoxLayout(toolbar);
        tbLayout->setContentsMargins(0, 0, 0, 0);
        tbLayout->setSpacing(6);

        auto *tmplLabel = new QLabel(tr("模板:"), toolbar);
        tbLayout->addWidget(tmplLabel);

        m_templateCombo = new QComboBox(toolbar);
        m_templateCombo->setObjectName(QStringLiteral("protocolTemplateCombo"));
        m_templateCombo->setMinimumWidth(180);
        tbLayout->addWidget(m_templateCombo);

        m_addBtn = new QPushButton(tr("+新建"), toolbar);
        m_addBtn->setObjectName(QStringLiteral("protocolAddTemplateButton"));
        tbLayout->addWidget(m_addBtn);

        m_editBtn = new QPushButton(tr("编辑"), toolbar);
        m_editBtn->setObjectName(QStringLiteral("protocolEditTemplateButton"));
        tbLayout->addWidget(m_editBtn);

        m_deleteBtn = new QPushButton(tr("删除"), toolbar);
        m_deleteBtn->setObjectName(QStringLiteral("protocolDeleteTemplateButton"));
        tbLayout->addWidget(m_deleteBtn);

        tbLayout->addSpacing(12);

        m_pauseBtn = new QToolButton(toolbar);
        m_pauseBtn->setText(tr("暂停"));
        m_pauseBtn->setCheckable(true);
        m_pauseBtn->setObjectName(QStringLiteral("protocolPauseButton"));
        tbLayout->addWidget(m_pauseBtn);

        m_clearBtn = new QPushButton(tr("清空"), toolbar);
        m_clearBtn->setObjectName(QStringLiteral("protocolClearButton"));
        tbLayout->addWidget(m_clearBtn);

        tbLayout->addStretch(1);

        m_statusLabel = new QLabel(tr("就绪"), toolbar);
        m_statusLabel->setObjectName(QStringLiteral("protocolStatusLabel"));
        tbLayout->addWidget(m_statusLabel);

        auto *rootLayout = qobject_cast<QVBoxLayout *>(layout());
        if (rootLayout)
            rootLayout->addWidget(toolbar);

        // --- 上下分栏：帧列表 + 帧详情 ---
        auto *splitter = new QSplitter(Qt::Vertical, this);

        // 帧列表
        m_frameList = new QTableWidget(splitter);
        m_frameList->setObjectName(QStringLiteral("protoFrameList"));
        m_frameList->setColumnCount(FL_FirstField + 3); // 初始列，动态调整
        m_frameList->setHorizontalHeaderLabels({
            tr("#"), tr("时间戳"), tr("模板"), tr("有效"), tr("校验"), tr("字段1")
        });
        m_frameList->horizontalHeader()->setSectionResizeMode(FL_Index, QHeaderView::ResizeToContents);
        m_frameList->horizontalHeader()->setSectionResizeMode(FL_Timestamp, QHeaderView::ResizeToContents);
        m_frameList->horizontalHeader()->setSectionResizeMode(FL_Template, QHeaderView::ResizeToContents);
        m_frameList->horizontalHeader()->setSectionResizeMode(FL_Valid, QHeaderView::ResizeToContents);
        m_frameList->horizontalHeader()->setSectionResizeMode(FL_Checksum, QHeaderView::ResizeToContents);
        m_frameList->setSelectionBehavior(QAbstractItemView::SelectRows);
        m_frameList->setSelectionMode(QAbstractItemView::SingleSelection);
        m_frameList->setEditTriggers(QAbstractItemView::NoEditTriggers);
        m_frameList->verticalHeader()->setVisible(false);
        m_frameList->setAlternatingRowColors(true);

        // 帧详情
        m_frameDetail = new QTableWidget(splitter);
        m_frameDetail->setObjectName(QStringLiteral("protoFrameDetail"));
        m_frameDetail->setColumnCount(DC_Count);
        m_frameDetail->setHorizontalHeaderLabels({
            tr("偏移"), tr("字段名"), tr("类型"), tr("HEX"),
            tr("DEC"), tr("其他"), tr("匹配")
        });
        m_frameDetail->horizontalHeader()->setSectionResizeMode(DC_Offset, QHeaderView::ResizeToContents);
        m_frameDetail->horizontalHeader()->setSectionResizeMode(DC_Name, QHeaderView::Stretch);
        m_frameDetail->horizontalHeader()->setSectionResizeMode(DC_Type, QHeaderView::ResizeToContents);
        m_frameDetail->horizontalHeader()->setSectionResizeMode(DC_Hex, QHeaderView::ResizeToContents);
        m_frameDetail->horizontalHeader()->setSectionResizeMode(DC_Dec, QHeaderView::ResizeToContents);
        m_frameDetail->horizontalHeader()->setSectionResizeMode(DC_Other, QHeaderView::ResizeToContents);
        m_frameDetail->horizontalHeader()->setSectionResizeMode(DC_Matched, QHeaderView::ResizeToContents);
        m_frameDetail->setSelectionBehavior(QAbstractItemView::SelectRows);
        m_frameDetail->setSelectionMode(QAbstractItemView::SingleSelection);
        m_frameDetail->setEditTriggers(QAbstractItemView::NoEditTriggers);
        m_frameDetail->verticalHeader()->setVisible(false);
        m_frameDetail->setAlternatingRowColors(true);

        splitter->addWidget(m_frameList);
        splitter->addWidget(m_frameDetail);
        splitter->setStretchFactor(0, 2);
        splitter->setStretchFactor(1, 1);

        if (rootLayout)
            rootLayout->addWidget(splitter, 1);

        // 连接
        connect(m_templateCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
                this, &ProtocolDecoderPage::onTemplateComboChanged);
        connect(m_addBtn, &QPushButton::clicked,
                this, &ProtocolDecoderPage::onAddTemplate);
        connect(m_editBtn, &QPushButton::clicked,
                this, &ProtocolDecoderPage::onEditTemplate);
        connect(m_deleteBtn, &QPushButton::clicked,
                this, &ProtocolDecoderPage::onDeleteTemplate);
        connect(m_pauseBtn, &QToolButton::toggled,
                this, &ProtocolDecoderPage::onTogglePause);
        connect(m_clearBtn, &QPushButton::clicked,
                this, &ProtocolDecoderPage::onClear);
        connect(m_frameList, &QTableWidget::currentCellChanged,
                this, &ProtocolDecoderPage::onFrameSelectionChanged);
    }

    void ProtocolDecoderPage::subscribeDataBus()
    {
        if (m_dataBus == nullptr)
            return;

        m_subscription = m_dataBus->subscribe(
            QStringLiteral("transport.serial.*"),
            [this](const DataPacket &packet)
            {
                if (m_paused || packet.rawPayload.isEmpty())
                    return;

                m_dataBuffer.append(packet.rawPayload);
                if (m_dataBuffer.size() > 65536)
                    m_dataBuffer = m_dataBuffer.right(32768);

                // 记录追加前的缓冲区长度，用于修正 streamOffset
                const int oldSize = m_dataBuffer.size() - packet.rawPayload.size();
                processData(m_dataBuffer, oldSize);
            });
    }

    void ProtocolDecoderPage::processData(const QByteArray &data, int bufferOffset)
    {
        if (m_decoder->templateCount() == 0)
            return;

        const int selectedTemplate = m_templateCombo->currentData().toInt();
        const auto frames = m_decoder->decode(selectedTemplate, data);

        if (!frames.isEmpty())
        {
            int lastEnd = 0;
            for (const auto &f : frames)
            {
                // streamOffset 是相对于传入 data 的偏移，加上 bufferOffset
                // 转换为相对于 m_dataBuffer 的绝对偏移
                const int absEnd = bufferOffset + f.streamOffset + f.rawFrame.size();
                if (f.streamOffset >= 0)
                    lastEnd = qMax(lastEnd, absEnd);
            }

            if (lastEnd > 0 && lastEnd <= m_dataBuffer.size())
                m_dataBuffer = m_dataBuffer.mid(lastEnd);

            updateFrameList(frames);
        }
    }

    void ProtocolDecoderPage::updateFrameList(const QVector<DecodedFrame> &newFrames)
    {
        for (const auto &frame : newFrames)
        {
            m_recentFrames.append(frame);
        }

        // 限制最大数量
        while (m_recentFrames.size() > kMaxFrames)
            m_recentFrames.removeFirst();

        m_frameList->setUpdatesEnabled(false);
        m_frameList->setRowCount(0);
        m_frameList->setRowCount(m_recentFrames.size());

        for (int row = 0; row < m_recentFrames.size(); ++row)
        {
            const auto &frame = m_recentFrames[row];

            // 基础列
            auto *idxItem = new QTableWidgetItem(QString::number(row + 1));
            idxItem->setTextAlignment(Qt::AlignCenter);
            m_frameList->setItem(row, FL_Index, idxItem);

            const QString timeStr = frame.timestamp > 0
                ? QDateTime::fromMSecsSinceEpoch(frame.timestamp)
                      .toString(QStringLiteral("HH:mm:ss.zzz"))
                : QStringLiteral("--");
            auto *tsItem = new QTableWidgetItem(timeStr);
            m_frameList->setItem(row, FL_Timestamp, tsItem);

            auto *tmplItem = new QTableWidgetItem(frame.templateName);
            m_frameList->setItem(row, FL_Template, tmplItem);

            auto *validItem = new QTableWidgetItem(frame.valid ? tr("通过") : tr("失败"));
            validItem->setTextAlignment(Qt::AlignCenter);
            m_frameList->setItem(row, FL_Valid, validItem);

            auto *crcItem = new QTableWidgetItem(frame.checksumPassed ? tr("通过") : tr("未通过"));
            crcItem->setTextAlignment(Qt::AlignCenter);
            m_frameList->setItem(row, FL_Checksum, crcItem);

            // 动态字段列
            for (int col = 0; col < frame.fields.size(); ++col)
            {
                const int tableCol = FL_FirstField + col;
                // 确保有足够的列
                while (m_frameList->columnCount() <= tableCol)
                {
                    m_frameList->setColumnCount(m_frameList->columnCount() + 1);
                    m_frameList->setHorizontalHeaderItem(
                        tableCol, new QTableWidgetItem(frame.fields[col].fieldName));
                }

                const auto &df = frame.fields[col];
                QString val = df.hexValue;
                if (!df.matched)
                    val += QStringLiteral(" [不匹配]");

                auto *fieldItem = new QTableWidgetItem(val);
                fieldItem->setBackground(df.highlightColor);
                if (!df.matched)
                    fieldItem->setForeground(QColor(QStringLiteral("#D32F2F")));
                m_frameList->setItem(row, tableCol, fieldItem);

                // 设置列标题
                if (m_frameList->horizontalHeaderItem(tableCol) == nullptr)
                    m_frameList->setHorizontalHeaderItem(
                        tableCol, new QTableWidgetItem(df.fieldName));
            }
        }

        m_frameList->setUpdatesEnabled(true);
        m_statusLabel->setText(tr("共 %1 帧").arg(m_recentFrames.size()));
    }

    void ProtocolDecoderPage::updateFrameDetail(const DecodedFrame &frame)
    {
        m_frameDetail->setUpdatesEnabled(false);
        m_frameDetail->setRowCount(0);
        m_frameDetail->setRowCount(frame.fields.size());

        for (int row = 0; row < frame.fields.size(); ++row)
        {
            const auto &df = frame.fields[row];

            auto *offsetItem = new QTableWidgetItem(
                df.size > 1
                    ? QStringLiteral("%1-%2").arg(df.offset).arg(df.offset + df.size - 1)
                    : QString::number(df.offset));
            offsetItem->setTextAlignment(Qt::AlignCenter);
            m_frameDetail->setItem(row, DC_Offset, offsetItem);

            auto *nameItem = new QTableWidgetItem(df.fieldName);
            m_frameDetail->setItem(row, DC_Name, nameItem);

            auto *typeItem = new QTableWidgetItem(fieldTypeToString(df.type));
            m_frameDetail->setItem(row, DC_Type, typeItem);

            auto *hexItem = new QTableWidgetItem(df.hexValue);
            hexItem->setFont(QFont(QStringLiteral("Consolas"), 10));
            m_frameDetail->setItem(row, DC_Hex, hexItem);

            auto *decItem = new QTableWidgetItem(df.decValue);
            m_frameDetail->setItem(row, DC_Dec, decItem);

            auto *otherItem = new QTableWidgetItem(df.otherValue);
            m_frameDetail->setItem(row, DC_Other, otherItem);

            auto *matchItem = new QTableWidgetItem(df.matched ? tr("通过") : tr("失败"));
            matchItem->setTextAlignment(Qt::AlignCenter);
            if (!df.matched)
                matchItem->setForeground(QColor(QStringLiteral("#D32F2F")));
            m_frameDetail->setItem(row, DC_Matched, matchItem);

            // 行背景色
            for (int col = 0; col < DC_Count; ++col)
            {
                QTableWidgetItem *item = m_frameDetail->item(row, col);
                if (item)
                    item->setBackground(df.highlightColor);
            }
        }

        m_frameDetail->setUpdatesEnabled(true);
    }

    void ProtocolDecoderPage::refreshTemplateCombo()
    {
        m_templateCombo->blockSignals(true);
        m_templateCombo->clear();

        const auto &tmpls = m_decoder->templates();
        if (!tmpls.isEmpty())
            m_templateCombo->addItem(tr("全部模板"), -1);

        if (tmpls.isEmpty())
        {
            m_templateCombo->addItem(tr("(无模板)"), -1);
            m_editBtn->setEnabled(false);
            m_deleteBtn->setEnabled(false);
        }
        else
        {
            for (int i = 0; i < tmpls.size(); ++i)
                m_templateCombo->addItem(tmpls[i].name, i);
            m_editBtn->setEnabled(true);
            m_deleteBtn->setEnabled(true);
        }

        m_templateCombo->blockSignals(false);
    }

    // ---------------------------------------------------------------------------
    // Slots
    // ---------------------------------------------------------------------------

    void ProtocolDecoderPage::onTemplateComboChanged(int /*index*/)
    {
        // 模板切换不影响已有解析数据
    }

    void ProtocolDecoderPage::onLoadTemplates()
    {
        const auto tmpls = ProtocolTemplate::loadList();
        m_decoder->clearTemplates();
        for (const auto &t : tmpls)
            m_decoder->addTemplate(t);
        refreshTemplateCombo();
        emit statusMessageGenerated(
            tr("已加载 %1 个协议模板").arg(tmpls.size()));
    }

    void ProtocolDecoderPage::onAddTemplate()
    {
        ProtocolTemplateEditor editor(this);
        if (editor.exec() == QDialog::Accepted)
        {
            ProtocolTemplate tmpl = editor.getTemplate();
            const int idx = m_decoder->addTemplate(tmpl);
        if (idx >= 0)
            {
                // 保存
                ProtocolTemplate::saveList(m_decoder->templates());
                refreshTemplateCombo();
                m_templateCombo->setCurrentIndex(idx + 1);
                emit statusMessageGenerated(
                    tr("已添加模板: %1").arg(tmpl.name));
            }
        }
    }

    void ProtocolDecoderPage::onEditTemplate()
    {
        const int idx = m_templateCombo->currentData().toInt();
        const auto &tmpls = m_decoder->templates();
        if (idx < 0 || idx >= tmpls.size())
            return;

        ProtocolTemplateEditor editor(this);
        editor.setTemplate(tmpls[idx]);
        if (editor.exec() == QDialog::Accepted)
        {
            // 替换模板
            m_decoder->removeTemplate(idx);
            ProtocolTemplate newTmpl = editor.getTemplate();
            const int newIndex = m_decoder->addTemplate(newTmpl);
            ProtocolTemplate::saveList(m_decoder->templates());
            refreshTemplateCombo();
            if (newIndex >= 0)
                m_templateCombo->setCurrentIndex(newIndex + 1);
            emit statusMessageGenerated(
                tr("已更新模板: %1").arg(newTmpl.name));
        }
    }

    void ProtocolDecoderPage::onDeleteTemplate()
    {
        const int idx = m_templateCombo->currentData().toInt();
        if (idx < 0 || idx >= m_decoder->templateCount())
            return;

        const QString name = m_decoder->templates()[idx].name;
        auto result = QMessageBox::question(this, tr("确认删除"),
            tr("确定删除模板 \"%1\" 吗？").arg(name));
        if (result == QMessageBox::Yes)
        {
            m_decoder->removeTemplate(idx);
            ProtocolTemplate::saveList(m_decoder->templates());
            refreshTemplateCombo();
            emit statusMessageGenerated(
                tr("已删除模板: %1").arg(name));
        }
    }

    void ProtocolDecoderPage::onTogglePause()
    {
        m_paused = m_pauseBtn->isChecked();
        m_pauseBtn->setText(m_paused ? tr("继续") : tr("暂停"));
        emit statusMessageGenerated(
            m_paused ? tr("协议解析已暂停") : tr("协议解析已继续"));
    }

    void ProtocolDecoderPage::onClear()
    {
        m_dataBuffer.clear();
        m_recentFrames.clear();
        m_frameList->setRowCount(0);
        m_frameDetail->setRowCount(0);
        m_frameCounter = 0;
        m_statusLabel->setText(tr("已清空"));
        emit statusMessageGenerated(tr("协议数据已清空"));
    }

    void ProtocolDecoderPage::onFrameSelectionChanged()
    {
        int row = m_frameList->currentRow();
        if (row < 0 || row >= m_recentFrames.size())
        {
            m_frameDetail->setRowCount(0);
            return;
        }

        updateFrameDetail(m_recentFrames[row]);
    }

} // namespace est
