#include "pages/SerialPortEnumerator.h"

#include <QDebug>
#include <QList>
#include <QSerialPortInfo>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

namespace est
{

    namespace
    {
#ifdef Q_OS_WIN
        /// 核心枚举函数：不含 __try，直接调用 availablePorts()
        static void doAvailablePorts(QList<QSerialPortInfo> *outResult)
        {
            *outResult = QSerialPortInfo::availablePorts();
        }

        /// SEH 包装：此函数不含任何需要析构的局部 C++ 对象
        /// 所有 C++ 对象都在堆上分配，通过裸指针操作
        static bool sehAvailablePorts(QList<QSerialPortInfo> *outResult)
        {
            __try {
                doAvailablePorts(outResult);
                return true;
            } __except (EXCEPTION_EXECUTE_HANDLER) {
                return false;
            }
        }
#endif
    }

    QList<QSerialPortInfo> safeAvailablePorts()
    {
#ifdef Q_OS_WIN
        QList<QSerialPortInfo> *heapResult = new QList<QSerialPortInfo>();
        const bool ok = sehAvailablePorts(heapResult);
        QList<QSerialPortInfo> result;
        if (ok) {
            result = *heapResult;
        }
        delete heapResult;
        if (!ok) {
            qWarning() << "QSerialPortInfo::availablePorts() triggered SEH exception (bad USB-serial driver?)";
        }
        return result;
#else
        // 非 Windows 平台直接调用 QSerialPortInfo::availablePorts()
        // 不做 SEH 异常保护，因为 MSVC __try/__except 不可用。
        // 这是已知的平台限制：若底层驱动异常触发信号，QSerialPortInfo
        // 可能崩溃，但 Linux/macOS 串口驱动通常比 Windows 稳定。
        return QSerialPortInfo::availablePorts();
#endif
    }

} // namespace est
