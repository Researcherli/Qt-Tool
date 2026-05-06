#ifndef EST_MODULEPLACEHOLDERPAGE_H
#define EST_MODULEPLACEHOLDERPAGE_H

#include <QWidget>

namespace est
{

    class ModulePlaceholderPage : public QWidget
    {
        Q_OBJECT

    public:
        explicit ModulePlaceholderPage(const QString &title,
                                       const QString &description,
                                       const QString &badgeText,
                                       QWidget *parent = nullptr);
    };

} // namespace est

#endif // EST_MODULEPLACEHOLDERPAGE_H
