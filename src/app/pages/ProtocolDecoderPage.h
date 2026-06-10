#ifndef EST_PROTOCOLDECODERPAGE_H
#define EST_PROTOCOLDECODERPAGE_H

#include "services/ProtocolDecoder.h"

#include <QWidget>

#include "databus/SubscriptionHandle.h"

class QComboBox;
class QLabel;
class QPushButton;
class QTableWidget;
class QToolButton;

namespace est
{

    class DataBus;
    class ICore;
    struct DataPacket;

    /**
     * 协议解析器页面 — 订阅串口数据，按自定义模板解析二进制帧并展示。
     */
    class ProtocolDecoderPage : public QWidget
    {
        Q_OBJECT

    public:
        explicit ProtocolDecoderPage(ICore *core, QWidget *parent = nullptr);
        ~ProtocolDecoderPage() override;

    signals:
        void statusMessageGenerated(const QString &text);

    private slots:
        void onTemplateComboChanged(int index);
        void onAddTemplate();
        void onEditTemplate();
        void onDeleteTemplate();
        void onTogglePause();
        void onClear();
        void onFrameSelectionChanged();
        void onLoadTemplates();

    private:
        void setupUi();
        void subscribeDataBus();
        void processData(const QByteArray &data, int bufferOffset);
        void updateFrameList(const QVector<DecodedFrame> &frames);
        void updateFrameDetail(const DecodedFrame &frame);
        void refreshTemplateCombo();

        // 枚举
        enum FrameListColumn
        {
            FL_Index = 0,
            FL_Timestamp,
            FL_Template,
            FL_Valid,
            FL_Checksum,
            FL_FirstField,
        };

        enum DetailColumn
        {
            DC_Offset = 0,
            DC_Name,
            DC_Type,
            DC_Hex,
            DC_Dec,
            DC_Other,
            DC_Matched,
            DC_Count
        };

        DataBus *m_dataBus = nullptr;
        SubscriptionHandle m_subscription;
        ProtocolDecoder *m_decoder = nullptr;

        // UI
        QComboBox *m_templateCombo = nullptr;
        QPushButton *m_addBtn = nullptr;
        QPushButton *m_editBtn = nullptr;
        QPushButton *m_deleteBtn = nullptr;
        QToolButton *m_pauseBtn = nullptr;
        QPushButton *m_clearBtn = nullptr;

        QTableWidget *m_frameList = nullptr;
        QTableWidget *m_frameDetail = nullptr;
        QLabel *m_statusLabel = nullptr;

        // 数据
        QByteArray m_dataBuffer;
        bool m_paused = false;
        int m_frameCounter = 0;

        // 存储最近帧
        QVector<DecodedFrame> m_recentFrames;
        static constexpr int kMaxFrames = 500;
    };

} // namespace est

#endif // EST_PROTOCOLDECODERPAGE_H
