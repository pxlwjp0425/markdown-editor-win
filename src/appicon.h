#ifndef APPICON_H
#define APPICON_H

#include <QIcon>
#include <QSize>
#include <QString>

/**
 * 构造应用图标。
 *
 * 为什么按尺寸分别绑定两份 SVG：
 *   完整版图标是「M + 向下箭头」（Markdown 的标准符号），
 *   但箭头在 16/24/32 像素下会糊成一团白点，反而让 M 看不清。
 *   所以小尺寸改用只有粗体 M 的简化版，保证任务栏和标题栏里可辨。
 *
 * 用 addFile 而不是直接 QIcon("xx.svg")，是为了让 Qt 在每种尺寸下
 * 都取到对应的源文件，而不是缩放同一份。
 */
inline QIcon makeAppIcon()
{
    QIcon icon;
    const QString small = QStringLiteral(":/icons/app-icon-small.svg");
    const QString full  = QStringLiteral(":/icons/app-icon.svg");

    icon.addFile(small, QSize(16, 16));
    icon.addFile(small, QSize(20, 20));
    icon.addFile(small, QSize(24, 24));
    icon.addFile(small, QSize(32, 32));

    icon.addFile(full,  QSize(40, 40));
    icon.addFile(full,  QSize(48, 48));
    icon.addFile(full,  QSize(64, 64));
    icon.addFile(full,  QSize(128, 128));
    icon.addFile(full,  QSize(256, 256));

    // 兜底：上面按尺寸精确匹配，但某些场景（如非整数缩放）会走这里
    icon.addFile(full,  QSize(512, 512));

    return icon;
}

#endif // APPICON_H
