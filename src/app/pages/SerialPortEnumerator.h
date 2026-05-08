#ifndef EST_SERIALPORTENUMERATOR_H
#define EST_SERIALPORTENUMERATOR_H

#include <QList>
#include <QSerialPortInfo>

namespace est
{

    /// 安全枚举串口：在 Windows 上使用 SEH 保护，防止 USB-串口驱动异常导致进程崩溃
    QList<QSerialPortInfo> safeAvailablePorts();

} // namespace est

#endif // EST_SERIALPORTENUMERATOR_H
