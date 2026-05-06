#include "widgets/DataConvertWidget.h"

#include "services/ByteFormatService.h"
#include "services/DataConvertService.h"

#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QComboBox>
#include <QDragEnterEvent>
#include <QFile>
#include <QFileDialog>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QMimeData>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTextStream>
#include <QUrl>
#include <QVBoxLayout>

namespace est
{

    DataConvertWidget::DataConvertWidget(QWidget *parent)
        : QWidget(parent)
    {
        setAcceptDrops(true);

        auto *rootLayout = new QVBoxLayout(this);
        rootLayout->setContentsMargins(0, 0, 0, 0);
        rootLayout->setSpacing(12);

        auto *configGroup = new QGroupBox(tr("转换配置"), this);
        auto *configLayout = new QGridLayout(configGroup);

        m_inputTypeCombo = new QComboBox(configGroup);
        m_inputTypeCombo->addItem(tr("字符串"), QStringLiteral("string"));
        m_inputTypeCombo->addItem(QStringLiteral("HEX"), QStringLiteral("hex"));
        m_inputTypeCombo->addItem(tr("二进制"), QStringLiteral("binary"));
        m_inputTypeCombo->addItem(QStringLiteral("Base64"), QStringLiteral("base64"));
        m_inputTypeCombo->addItem(tr("十进制字节"), QStringLiteral("decimal"));

        m_outputTypeCombo = new QComboBox(configGroup);
        m_outputTypeCombo->addItem(QStringLiteral("HEX"), QStringLiteral("hex"));
        m_outputTypeCombo->addItem(tr("字符串"), QStringLiteral("string"));
        m_outputTypeCombo->addItem(tr("二进制"), QStringLiteral("binary"));
        m_outputTypeCombo->addItem(QStringLiteral("Base64"), QStringLiteral("base64"));
        m_outputTypeCombo->addItem(tr("十进制字节"), QStringLiteral("decimal"));

        m_encodingCombo = new QComboBox(configGroup);
        m_encodingCombo->addItem(QStringLiteral("UTF-8"), QStringLiteral("utf8"));
        m_encodingCombo->addItem(QStringLiteral("ASCII"), QStringLiteral("ascii"));
        m_encodingCombo->addItem(QStringLiteral("GBK"), QStringLiteral("gbk"));
        m_encodingCombo->addItem(QStringLiteral("UTF-16 LE"), QStringLiteral("utf16le"));
        m_encodingCombo->addItem(QStringLiteral("UTF-16 BE"), QStringLiteral("utf16be"));

        m_separatorCombo = new QComboBox(configGroup);
        m_separatorCombo->addItem(tr("空格"), QStringLiteral("space"));
        m_separatorCombo->addItem(tr("无分隔"), QStringLiteral("none"));
        m_separatorCombo->addItem(QStringLiteral("0x 前缀"), QStringLiteral("0x"));
        m_separatorCombo->addItem(tr("逗号"), QStringLiteral("comma"));

        m_caseCombo = new QComboBox(configGroup);
        m_caseCombo->addItem(tr("大写"), true);
        m_caseCombo->addItem(tr("小写"), false);

        m_autoConvertCheckBox = new QCheckBox(tr("自动转换"), configGroup);

        configLayout->addWidget(new QLabel(tr("输入类型"), configGroup), 0, 0);
        configLayout->addWidget(m_inputTypeCombo, 0, 1);
        configLayout->addWidget(new QLabel(tr("输出类型"), configGroup), 0, 2);
        configLayout->addWidget(m_outputTypeCombo, 0, 3);
        configLayout->addWidget(new QLabel(tr("编码"), configGroup), 1, 0);
        configLayout->addWidget(m_encodingCombo, 1, 1);
        configLayout->addWidget(new QLabel(tr("HEX 分隔"), configGroup), 1, 2);
        configLayout->addWidget(m_separatorCombo, 1, 3);
        configLayout->addWidget(new QLabel(tr("大小写"), configGroup), 2, 0);
        configLayout->addWidget(m_caseCombo, 2, 1);
        configLayout->addWidget(m_autoConvertCheckBox, 2, 3);

        auto *editLayout = new QHBoxLayout();
        editLayout->setSpacing(12);

        auto *inputGroup = new QGroupBox(tr("原始数据"), this);
        auto *inputLayout = new QVBoxLayout(inputGroup);
        m_inputEdit = new QPlainTextEdit(inputGroup);
        m_inputEdit->setPlaceholderText(tr("输入要转换的内容"));
        m_inputLengthLabel = new QLabel(tr("输入长度：0"), inputGroup);
        auto *pasteButton = new QPushButton(tr("粘贴"), inputGroup);
        auto *clearInputButton = new QPushButton(tr("清空输入"), inputGroup);
        auto *inputButtonLayout = new QHBoxLayout();
        inputButtonLayout->addWidget(m_inputLengthLabel);
        inputButtonLayout->addStretch(1);
        inputButtonLayout->addWidget(pasteButton);
        inputButtonLayout->addWidget(clearInputButton);
        inputLayout->addWidget(m_inputEdit, 1);
        inputLayout->addLayout(inputButtonLayout);

        auto *outputGroup = new QGroupBox(tr("转换结果"), this);
        auto *outputLayout = new QVBoxLayout(outputGroup);
        m_outputEdit = new QPlainTextEdit(outputGroup);
        m_outputEdit->setReadOnly(true);
        m_outputEdit->setPlaceholderText(tr("点击“转换”查看结果"));
        m_outputLengthLabel = new QLabel(tr("输出长度：0"), outputGroup);
        auto *copyButton = new QPushButton(tr("复制结果"), outputGroup);
        auto *saveButton = new QPushButton(tr("保存结果"), outputGroup);
        auto *outputButtonLayout = new QHBoxLayout();
        outputButtonLayout->addWidget(m_outputLengthLabel);
        outputButtonLayout->addStretch(1);
        outputButtonLayout->addWidget(copyButton);
        outputButtonLayout->addWidget(saveButton);
        outputLayout->addWidget(m_outputEdit, 1);
        outputLayout->addLayout(outputButtonLayout);

        editLayout->addWidget(inputGroup, 1);
        editLayout->addWidget(outputGroup, 1);

        auto *actionLayout = new QHBoxLayout();
        auto *convertButton = new QPushButton(tr("转换"), this);
        convertButton->setObjectName(QStringLiteral("primaryActionButton"));
        auto *clearAllButton = new QPushButton(tr("清空全部"), this);
        auto *swapButton = new QPushButton(tr("交换输入输出"), this);
        auto *exampleButton = new QPushButton(tr("示例"), this);
        actionLayout->addWidget(convertButton);
        actionLayout->addWidget(clearAllButton);
        actionLayout->addWidget(swapButton);
        actionLayout->addWidget(exampleButton);
        actionLayout->addStretch(1);

        rootLayout->addWidget(configGroup);
        rootLayout->addLayout(editLayout, 1);
        rootLayout->addLayout(actionLayout);

        connect(convertButton, &QPushButton::clicked, this, &DataConvertWidget::performConversion);
        connect(clearAllButton, &QPushButton::clicked, this, &DataConvertWidget::clearAll);
        connect(swapButton, &QPushButton::clicked, this, &DataConvertWidget::swapInputOutput);
        connect(exampleButton, &QPushButton::clicked, this, [this]() {
            m_inputEdit->setPlainText(QStringLiteral("JCXX_IDENTITY"));
            performConversion();
        });
        connect(pasteButton, &QPushButton::clicked, this, [this]() {
            m_inputEdit->setPlainText(QApplication::clipboard()->text());
            if (m_autoConvertCheckBox->isChecked())
            {
                performConversion();
            }
        });
        connect(clearInputButton, &QPushButton::clicked, this, [this]() {
            m_inputEdit->clear();
            updateLengthLabels();
        });
        connect(copyButton, &QPushButton::clicked, this, [this]() {
            QApplication::clipboard()->setText(m_outputEdit->toPlainText());
            emit statusMessageGenerated(tr("转换结果已复制到剪贴板"));
        });
        connect(saveButton, &QPushButton::clicked, this, [this]() {
            const QString filePath = QFileDialog::getSaveFileName(this, tr("保存转换结果"), QStringLiteral("convert_result.txt"), tr("文本文件 (*.txt)"));
            if (filePath.isEmpty())
            {
                return;
            }
            QFile file(filePath);
            if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
            {
                emit statusMessageGenerated(tr("保存失败：无法写入 %1").arg(filePath));
                return;
            }
            QTextStream stream(&file);
            stream.setEncoding(QStringConverter::Utf8);
            stream << m_outputEdit->toPlainText();
            emit statusMessageGenerated(tr("转换结果已保存到：%1").arg(filePath));
        });

        auto autoConvertHandler = [this]() {
            updateLengthLabels();
            if (m_autoConvertCheckBox->isChecked())
            {
                performConversion();
            }
        };

        connect(m_inputEdit, &QPlainTextEdit::textChanged, this, autoConvertHandler);
        connect(m_inputTypeCombo, &QComboBox::currentTextChanged, this, autoConvertHandler);
        connect(m_outputTypeCombo, &QComboBox::currentTextChanged, this, autoConvertHandler);
        connect(m_encodingCombo, &QComboBox::currentTextChanged, this, autoConvertHandler);
        connect(m_separatorCombo, &QComboBox::currentTextChanged, this, autoConvertHandler);
        connect(m_caseCombo, &QComboBox::currentTextChanged, this, autoConvertHandler);

        updateLengthLabels();
    }

    void DataConvertWidget::dragEnterEvent(QDragEnterEvent *event)
    {
        if (event->mimeData()->hasUrls())
        {
            event->acceptProposedAction();
        }
    }

    void DataConvertWidget::dropEvent(QDropEvent *event)
    {
        const QList<QUrl> urls = event->mimeData()->urls();
        if (urls.size() != 1)
        {
            emit statusMessageGenerated(tr("一次只能拖入一个文件到数据转换。"));
            return;
        }

        const QString filePath = urls.first().toLocalFile();
        QFile file(filePath);
        if (filePath.isEmpty() || !file.open(QIODevice::ReadOnly))
        {
            emit statusMessageGenerated(tr("无法读取拖入的文件。"));
            return;
        }

        const QByteArray bytes = file.readAll();
        const int hexIndex = m_inputTypeCombo->findData(QStringLiteral("hex"));
        if (hexIndex >= 0)
        {
            m_inputTypeCombo->setCurrentIndex(hexIndex);
        }
        m_inputEdit->setPlainText(ByteFormatService::formatHex(bytes, QStringLiteral("space"), true));
        emit statusMessageGenerated(tr("已载入文件字节到数据转换输入区"));
        event->acceptProposedAction();
    }

    void DataConvertWidget::performConversion()
    {
        DataConvertService::ConversionOptions options;
        options.inputFormat = DataConvertService::dataFormatFromKey(m_inputTypeCombo->currentData().toString());
        options.outputFormat = DataConvertService::dataFormatFromKey(m_outputTypeCombo->currentData().toString());
        options.encoding = DataConvertService::textEncodingFromKey(m_encodingCombo->currentData().toString());
        options.hexSeparatorMode = m_separatorCombo->currentData().toString();
        options.upperCaseHex = m_caseCombo->currentData().toBool();

        QString result;
        QString errorMessage;
        if (!DataConvertService::convert(m_inputEdit->toPlainText(), options, &result, &errorMessage))
        {
            m_outputEdit->setPlainText(errorMessage);
            emit statusMessageGenerated(errorMessage);
            updateLengthLabels();
            return;
        }

        m_outputEdit->setPlainText(result);
        emit statusMessageGenerated(tr("转换完成"));
        updateLengthLabels();
    }

    void DataConvertWidget::updateLengthLabels()
    {
        m_inputLengthLabel->setText(tr("输入长度：%1").arg(m_inputEdit->toPlainText().size()));
        m_outputLengthLabel->setText(tr("输出长度：%1").arg(m_outputEdit->toPlainText().size()));
    }

    void DataConvertWidget::clearAll()
    {
        m_inputEdit->clear();
        m_outputEdit->clear();
        updateLengthLabels();
        emit statusMessageGenerated(tr("输入和输出已清空"));
    }

    void DataConvertWidget::swapInputOutput()
    {
        const QString inputText = m_inputEdit->toPlainText();
        m_inputEdit->setPlainText(m_outputEdit->toPlainText());
        m_outputEdit->setPlainText(inputText);
        updateLengthLabels();
        emit statusMessageGenerated(tr("输入与输出已交换"));
    }

} // namespace est
