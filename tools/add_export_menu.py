#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
把「文件 > 导出」改成子菜单（对齐 Typora），并实现统一的 exportTo 入口。

Typora 的导出子菜单：HTML / PDF / Word / OpenOffice / EPUB / LaTeX / RTF /
OPML / Wiki / reStructuredText / Textile / Image / Markdown / Plain Text
"""
import io

# ===========================================================================
# 1) mainwindow.h：加声明
# ===========================================================================
ph = 'src/mainwindow.h'
h = io.open(ph, encoding='utf-8').read()

if 'void exportTo(const QString' not in h:
    h = h.replace('    void exportToHtml();\n    void exportToPdf();',
                  '''    void exportToHtml();
    void exportToPdf();

    // 统一导出入口：按格式 id 导出（见 Exporter::formats()）
    void exportTo(const QString &formatId);
    void exportToPdfFile(const QString &fileName);
    void exportToOdtFile(const QString &fileName);
    void exportToImageFile(const QString &fileName);''')
    print('  mainwindow.h 已加声明')

    # action 成员
    h = h.replace('    QAction *m_preferencesFileAction;',
                  '    QAction *m_preferencesFileAction;\n'
                  '    QMenu   *m_exportMenu;   // 文件 > 导出 子菜单')
    io.open(ph, 'w', encoding='utf-8', newline='\n').write(h)

# ===========================================================================
# 2) mainwindow.cpp：include
# ===========================================================================
pc = 'src/mainwindow.cpp'
s = io.open(pc, encoding='utf-8').read()

if '#include "exporter.h"' not in s:
    s = s.replace('#include "blockeditor.h"', '#include "blockeditor.h"\n#include "exporter.h"')
    print('  include 已加')

# ===========================================================================
# 3) createMenus：导出改子菜单
# ===========================================================================
old_menu = '''    fileMenu->addAction(m_exportHtmlAction);
    fileMenu->addAction(m_printAction);
    fileMenu->addAction(m_exportPdfAction);'''

new_menu = '''    // 「导出」子菜单（对齐 Typora 的 14 种格式）
    m_exportMenu = fileMenu->addMenu(tr("导出"));
    for (const Exporter::Format &f : Exporter::formats()) {
        QAction *act = m_exportMenu->addAction(f.displayName + QStringLiteral("..."));
        const QString fid = f.id;
        connect(act, &QAction::triggered, this, [this, fid]() { exportTo(fid); });
    }
    fileMenu->addSeparator();
    fileMenu->addAction(m_printAction);'''

if 'm_exportMenu = fileMenu->addMenu' not in s:
    assert old_menu in s, '导出菜单块未匹配'
    s = s.replace(old_menu, new_menu, 1)
    print('  导出子菜单已建立')

io.open(pc, 'w', encoding='utf-8', newline='\n').write(s)
print('done')
