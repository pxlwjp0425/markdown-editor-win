#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
加一个截图辅助钩子 --show-export-menu：
程序启动后自动把「文件 > 导出」子菜单弹出来，便于截图展示 14 种格式。
"""
import io

# ---- mainwindow.h ----
ph = 'src/mainwindow.h'
h = io.open(ph, encoding='utf-8').read()
if 'showExportMenuForScreenshot' not in h:
    h = h.replace('    void exportTo(const QString &formatId);',
                  '    void exportTo(const QString &formatId);\n\n'
                  '    // 仅供截图：把「文件 > 导出」子菜单弹出来\n'
                  '    void showExportMenuForScreenshot();')
    io.open(ph, 'w', encoding='utf-8', newline='\n').write(h)
    print('  h ok')

# ---- mainwindow.cpp ----
pc = 'src/mainwindow.cpp'
s = io.open(pc, encoding='utf-8').read()
if 'showExportMenuForScreenshot' not in s:
    IMPL = r'''

/**
 * 仅用于截图展示：把「文件 > 导出」子菜单弹出来。
 *
 * 用 QMenu::popup 而不是 exec —— popup 是非阻塞的，
 * 截图脚本可以在外部抓到菜单，程序也不会卡住。
 */
void MainWindow::showExportMenuForScreenshot()
{
    if (!m_exportMenu) {
        return;
    }
    // 找到「文件」菜单，用它作为父级定位
    QMenu *fileMenu = nullptr;
    for (QAction *a : menuBar()->actions()) {
        if (a->menu() && a->text().contains(tr("文件"))) {
            fileMenu = a->menu();
            break;
        }
    }
    if (!fileMenu) {
        return;
    }
    // 在文件菜单下方弹出导出子菜单
    const QPoint pos = menuBar()->mapToGlobal(QPoint(0, menuBar()->height()));
    m_exportMenu->popup(pos + QPoint(20, 30));
}
'''
    s = s.rstrip() + '\n' + IMPL
    io.open(pc, 'w', encoding='utf-8', newline='\n').write(s)
    print('  cpp ok')

# ---- main.cpp：注册选项 ----
pm = 'src/main.cpp'
m = io.open(pm, encoding='utf-8').read()
if 'show-export-menu' not in m:
    old = '''    QCommandLineOption probeOption(QStringLiteral("probe-export"), QStringLiteral("probe"));'''
    new = '''    QCommandLineOption showExportMenuOption(
        QStringList() << "show-export-menu",
        "截图辅助：启动后弹出「文件 > 导出」子菜单");
    parser.addOption(showExportMenuOption);

    QCommandLineOption probeOption(QStringLiteral("probe-export"), QStringLiteral("probe"));'''
    assert old in m
    m = m.replace(old, new, 1)

    # 处理：在 GUI 正常流程里，show 之后延迟弹出
    old2 = '''    w.show();'''
    # 只替换 GUI 模式下第一处
    idx = m.find('    // ---- 正常 GUI 模式 ----')
    seg = m[idx:]
    new2 = '''    w.show();'''
    io.open(pm, 'w', encoding='utf-8', newline='\n').write(m)
    print('  main.cpp 选项已加')
