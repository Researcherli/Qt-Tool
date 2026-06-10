#ifndef EST_PROTOCOLTEMPLATEEDITOR_H
#define EST_PROTOCOLTEMPLATEEDITOR_H

#include "services/ProtocolTemplate.h"

#include <QDialog>
#include <QVector>

class QComboBox;
class QLineEdit;
class QPushButton;
class QSpinBox;
class QTableWidget;

namespace est
{

    /**
     * 协议模板编辑对话框 — 新建/编辑二进制协议模板。
     *
     * 包含：模板名称、字节序、校验算法、字段列表（名称/类型/偏移/大小/期望值/格式/颜色）
     */
    class ProtocolTemplateEditor : public QDialog
    {
        Q_OBJECT

    public:
        explicit ProtocolTemplateEditor(QWidget *parent = nullptr);

        /// 编辑现有模板
        void setTemplate(const ProtocolTemplate &tmpl);

        /// 获取编辑后的模板
        ProtocolTemplate getTemplate() const;

    private slots:
        void onAddField();
        void onRemoveField();
        void onMoveFieldUp();
        void onMoveFieldDown();
        void onAccept();

    private:
        void setupUi();
        void refreshFieldTable();
        void readFieldsFromTable();

        QLineEdit *m_nameEdit = nullptr;
        QLineEdit *m_descEdit = nullptr;
        QComboBox *m_endianCombo = nullptr;
        QComboBox *m_checksumCombo = nullptr;
        QSpinBox *m_maxFrameSpin = nullptr;
        QTableWidget *m_fieldTable = nullptr;

        QVector<FieldDef> m_fields;
        bool m_editing = false;
    };

} // namespace est

#endif // EST_PROTOCOLTEMPLATEEDITOR_H
