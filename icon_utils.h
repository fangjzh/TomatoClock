#ifndef ICON_UTILS_H
#define ICON_UTILS_H

#include <QIcon>

class QStyle;
class QString;

QIcon loadRuntimeAppIconFromDisk(const QString &appDirPath, const QStyle *fallbackStyle);

#endif // ICON_UTILS_H
