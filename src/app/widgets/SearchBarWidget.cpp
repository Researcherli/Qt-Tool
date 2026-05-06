#include "widgets/SearchBarWidget.h"

#include <QCheckBox>
#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>

namespace est
{

    SearchBarWidget::SearchBarWidget(QWidget *parent)
        : QWidget(parent)
    {
        auto *layout = new QHBoxLayout(this);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(8);

        m_typeCombo = new QComboBox(this);
        m_typeCombo->addItem(tr("字符串"), static_cast<int>(BinSearchService::SearchType::String));
        m_typeCombo->addItem(tr("HEX"), static_cast<int>(BinSearchService::SearchType::Hex));
        m_typeCombo->addItem(tr("偏移"), static_cast<int>(BinSearchService::SearchType::Offset));

        m_inputEdit = new QLineEdit(this);
        m_inputEdit->setPlaceholderText(tr("输入搜索内容，如 JCXX / 4A 43 58 58 / 0x1200"));

        m_encodingCombo = new QComboBox(this);
        m_encodingCombo->addItem(QStringLiteral("UTF-8"), QStringLiteral("utf8"));
        m_encodingCombo->addItem(QStringLiteral("ASCII"), QStringLiteral("ascii"));
        m_encodingCombo->addItem(QStringLiteral("GBK"), QStringLiteral("gbk"));
        m_encodingCombo->addItem(QStringLiteral("UTF-16 LE"), QStringLiteral("utf16le"));
        m_encodingCombo->addItem(QStringLiteral("UTF-16 BE"), QStringLiteral("utf16be"));

        auto *searchButton = new QPushButton(tr("搜索"), this);
        auto *prevButton = new QPushButton(tr("上一个"), this);
        auto *nextButton = new QPushButton(tr("下一个"), this);
        auto *clearButton = new QPushButton(tr("清除"), this);
        m_resultLabel = new QLabel(tr("0 个结果"), this);
        m_highlightCheckBox = new QCheckBox(tr("高亮"), this);
        m_highlightCheckBox->setChecked(true);

        layout->addWidget(m_typeCombo);
        layout->addWidget(m_inputEdit, 1);
        layout->addWidget(m_encodingCombo);
        layout->addWidget(searchButton);
        layout->addWidget(prevButton);
        layout->addWidget(nextButton);
        layout->addWidget(m_resultLabel);
        layout->addWidget(m_highlightCheckBox);
        layout->addWidget(clearButton);

        connect(searchButton, &QPushButton::clicked, this, &SearchBarWidget::searchRequested);
        connect(prevButton, &QPushButton::clicked, this, &SearchBarWidget::previousRequested);
        connect(nextButton, &QPushButton::clicked, this, &SearchBarWidget::nextRequested);
        connect(clearButton, &QPushButton::clicked, this, [this]() {
            clearInput();
            emit clearRequested();
        });
        connect(m_inputEdit, &QLineEdit::returnPressed, this, &SearchBarWidget::searchRequested);
        connect(m_typeCombo, &QComboBox::currentIndexChanged, this, [this](int) {
            const auto type = static_cast<BinSearchService::SearchType>(m_typeCombo->currentData().toInt());
            m_encodingCombo->setEnabled(type == BinSearchService::SearchType::String);
        });
    }

    BinSearchService::SearchQuery SearchBarWidget::currentQuery() const
    {
        BinSearchService::SearchQuery query;
        query.type = static_cast<BinSearchService::SearchType>(m_typeCombo->currentData().toInt());
        query.input = m_inputEdit->text();
        query.encoding = ByteFormatService::textEncodingFromKey(m_encodingCombo->currentData().toString());
        return query;
    }

    bool SearchBarWidget::highlightEnabled() const
    {
        return m_highlightCheckBox->isChecked();
    }

    void SearchBarWidget::setResultCount(int count)
    {
        m_resultLabel->setText(tr("%1 个结果").arg(count));
    }

    void SearchBarWidget::clearInput()
    {
        m_inputEdit->clear();
        setResultCount(0);
    }

} // namespace est
