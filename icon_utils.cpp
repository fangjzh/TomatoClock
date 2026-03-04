#include "icon_utils.h"

#include <QDir>
#include <QFileInfo>
#include <QStringList>
#include <QStyle>

QIcon loadRuntimeAppIconFromDisk(const QString &appDirPath, const QStyle *fallbackStyle)
{
    const QStringList iconCandidates {
        QDir(appDirPath).filePath(QStringLiteral("resource/images/app_icon.ico")),
        QDir(appDirPath).filePath(QStringLiteral("resource/images/app_icon.png")),
        QDir(appDirPath).filePath(QStringLiteral("../resource/images/app_icon.ico")),
        QDir(appDirPath).filePath(QStringLiteral("../resource/images/app_icon.png"))
    };

    for (const QString &iconPath : iconCandidates) {
        if (!QFileInfo::exists(iconPath)) {
            continue;
        }

        QIcon icon(iconPath);
        if (!icon.isNull()) {
            return icon;
        }
    }

    if (fallbackStyle) {
        return fallbackStyle->standardIcon(QStyle::SP_ComputerIcon);
    }

    return QIcon();
}
