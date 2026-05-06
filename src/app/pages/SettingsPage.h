#ifndef EST_SETTINGSPAGE_H
#define EST_SETTINGSPAGE_H

#include <QWidget>

namespace est
{

    class SettingsPage : public QWidget
    {
        Q_OBJECT

    public:
        explicit SettingsPage(QWidget *parent = nullptr);
    };

} // namespace est

#endif // EST_SETTINGSPAGE_H
