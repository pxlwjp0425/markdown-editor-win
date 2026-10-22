#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
给程序加 --export-icon 模式：
把内置的两份 SVG（完整版 / 简化版）渲染成多尺寸 PNG，再拼成标准 ICO。

为什么自己拼 ICO：
  QImage::save("x.ico") 只能写单一尺寸，而 Windows 的 exe 图标需要
  16/24/32/48/64/128/256 多档，否则任务栏/资源管理器/大图标视图会模糊。
  ICO 格式本身很简单（ICONDIR + 若干 ICONDIRENTRY + PNG 数据），
  Vista 以后允许每个条目直接嵌 PNG，所以手拼即可。

尺寸策略（关键）：
  16/24/32 用简化版 —— 完整版的 "M↓" 在这个尺寸会糊成一团白点；
  48 以上用完整版 —— 有空间表达 M + 向下箭头（Markdown 的标准符号）。
"""
import io

pm = 'src/main.cpp'
m = io.open(pm, encoding='utf-8').read()

# ---- 1) 选项 ----
if 'export-icon' not in m:
    old = '''    QCommandLineOption showExportMenuOption('''
    new = '''    // 生成应用图标（多尺寸 ICO）
    QCommandLineOption exportIconOption(
        QStringList() << "export-icon",
        "把内置应用图标渲染为多尺寸 ICO 文件",
        "输出路径");
    parser.addOption(exportIconOption);

    QCommandLineOption showExportMenuOption('''
    assert old in m
    m = m.replace(old, new, 1)
    print('  选项已加')

# ---- 2) 处理逻辑：放在 QApplication 之后、MainWindow 之前 ----
old2 = '''    // ---- 自检：批量导出 ----'''
new2 = '''    // ---- 生成应用图标 ----
    if (parser.isSet(exportIconOption)) {
        QString err;
        const QString out = parser.value(exportIconOption);
        const bool ok = exportAppIcon(out, &err);
        QFile log(out + QStringLiteral(".log"));
        if (log.open(QIODevice::WriteOnly | QIODevice::Text)) {
            log.write(ok ? QByteArray("OK\\n") : QByteArray("FAIL: ") + err.toUtf8() + '\\n');
            log.close();
        }
        return ok ? 0 : 1;
    }

    // ---- 自检：批量导出 ----'''
assert old2 in m
m = m.replace(old2, new2, 1)

# ---- 3) 实现：插在 qMain 之前 ----
if 'exportAppIcon' not in m.split('int qMain')[0]:
    helper = r'''
/**
 * 把内置 SVG 渲染成多尺寸 PNG 并拼成 ICO。
 *
 * ICO 结构：
 *   ICONDIR      : reserved(2) type(2) count(2)
 *   ICONDIRENTRY : width(1) height(1) colors(1) reserved(1)
 *                  planes(2) bitcount(2) bytes(4) offset(4)
 *   data         : 每张图的 PNG 字节流
 * width/height 为 0 表示 256（字段只有 1 字节）。
 */
static bool exportAppIcon(const QString &outPath, QString *err)
{
    struct Spec {
        int size;
        const char *res;
    };
    // 小尺寸用简化版（只有 M），大尺寸用完整版（M + 向下箭头）
    static const Spec specs[] = {
        {16,  ":/icons/app-icon-small.svg"},
        {24,  ":/icons/app-icon-small.svg"},
        {32,  ":/icons/app-icon-small.svg"},
        {48,  ":/icons/app-icon.svg"},
        {64,  ":/icons/app-icon.svg"},
        {128, ":/icons/app-icon.svg"},
        {256, ":/icons/app-icon.svg"},
    };
    const int count = int(sizeof(specs) / sizeof(specs[0]));

    QList<QByteArray> pngs;
    for (int i = 0; i < count; ++i) {
        QSvgRenderer renderer(QString::fromLatin1(specs[i].res));
        if (!renderer.isValid()) {
            *err = QStringLiteral("无法加载 SVG: %1").arg(QString::fromLatin1(specs[i].res));
            return false;
        }
        QImage img(specs[i].size, specs[i].size, QImage::Format_ARGB32_Premultiplied);
        img.fill(Qt::transparent);
        QPainter painter(&img);
        painter.setRenderHint(QPainter::Antialiasing, true);
        painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
        renderer.render(&painter);
        painter.end();

        QByteArray png;
        QBuffer buffer(&png);
        buffer.open(QIODevice::WriteOnly);
        if (!img.save(&buffer, "PNG")) {
            *err = QStringLiteral("PNG 编码失败（size=%1）").arg(specs[i].size);
            return false;
        }
        buffer.close();
        pngs.append(png);
    }

    // 拼 ICO（小端）
    QByteArray ico;
    auto putU16 = [&ico](quint16 v) {
        ico.append(char(v & 0xFF));
        ico.append(char((v >> 8) & 0xFF));
    };
    auto putU32 = [&ico](quint32 v) {
        ico.append(char(v & 0xFF));
        ico.append(char((v >> 8) & 0xFF));
        ico.append(char((v >> 16) & 0xFF));
        ico.append(char((v >> 24) & 0xFF));
    };

    putU16(0);                       // reserved
    putU16(1);                       // type = icon
    putU16(static_cast<quint16>(count));

    quint32 offset = 6 + 16 * static_cast<quint32>(count);
    for (int i = 0; i < count; ++i) {
        const int s = specs[i].size;
        ico.append(char(s >= 256 ? 0 : s));      // width
        ico.append(char(s >= 256 ? 0 : s));      // height
        ico.append(char(0));                     // color count
        ico.append(char(0));                     // reserved
        putU16(1);                               // planes
        putU16(32);                              // bit count
        putU32(static_cast<quint32>(pngs.at(i).size()));
        putU32(offset);
        offset += static_cast<quint32>(pngs.at(i).size());
    }
    for (const QByteArray &png : pngs) {
        ico.append(png);
    }

    QFile file(outPath);
    if (!file.open(QIODevice::WriteOnly)) {
        *err = QStringLiteral("无法写入: %1").arg(file.errorString());
        return false;
    }
    const qint64 written = file.write(ico);
    file.close();
    if (written != ico.size()) {
        *err = QStringLiteral("写入不完整");
        return false;
    }
    return true;
}

'''
    idx = m.index('int qMain(')
    m = m[:idx] + helper + m[idx:]

# ---- 4) include ----
need = ['#include <QSvgRenderer>', '#include <QBuffer>', '#include <QPainter>', '#include <QImage>']
adds = [n for n in need if n not in m]
if adds:
    m = m.replace('#include <QIcon>', '#include <QIcon>\n' + '\n'.join(adds), 1)

io.open(pm, 'w', encoding='utf-8', newline='\n').write(m)
print('main.cpp 已加 --export-icon 与 exportAppIcon')
