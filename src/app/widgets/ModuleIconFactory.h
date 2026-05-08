#pragma once

#include <QPixmap>
#include <QString>

namespace est
{
    QPixmap makeModuleIconPixmap(const QString &type, int size);
    QPixmap makeHomeIconPixmap(int size);
}
