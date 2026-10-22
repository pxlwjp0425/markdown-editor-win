#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
给打开流程加分阶段计时打点（仅在 --bench-open 模式下生效）。

打点覆盖：
  read_file            读文件 + UTF-8 解码
  set_editor_text      QTextEdit 载入全文（含语法高亮）
  outline              大纲面板刷新
  preview_toPlainText  预览时重新取全文
  preview_generateHtml 生成 HTML
  preview_setHtml      QTextBrowser 渲染
  open_total           openFile 全程
  app_to_idle          进程启动 -> 事件循环首次空闲（用户可感知的启动时间）
"""
import io

# ===========================================================================
# 1) mainwindow.h：成员与方法
# ===========================================================================
ph = 'src/mainwindow.h'
h = io.open(ph, encoding='utf-8').read()

if 'm_benchTimer' not in h:
    h = h.replace('    // 打字机模式\n    bool m_typewriterMode;',
                  '''    // ---- 性能基准（--bench-open） ----
    bool m_benchMode;
    QElapsedTimer m_benchTimer;
    QStringList m_benchLog;

    // 打字机模式
    bool m_typewriterMode;''')
    h = h.replace('    void refreshDocTree();',
                  '''    void refreshDocTree();

    // ---- 性能基准 ----
    void benchStart();
    void benchMark(const QString &phase);
    QStringList benchLog() const { return m_benchLog; }
    void setBenchEnabled(bool on) { m_benchMode = on; }''')
    # include
    if '#include <QElapsedTimer>' not in h:
        h = h.replace('#include <QStringList>', '#include <QElapsedTimer>\n#include <QStringList>')
    io.open(ph, 'w', encoding='utf-8', newline='\n').write(h)
    print('  h ok')

# ===========================================================================
# 2) mainwindow.cpp：构造初始化
# ===========================================================================
pc = 'src/mainwindow.cpp'
s = io.open(pc, encoding='utf-8').read()

if 'm_benchMode(false)' not in s:
    s = s.replace('    , m_blockEditor(new BlockEditor())',
                  '    , m_blockEditor(new BlockEditor())\n    , m_benchMode(false)')
    print('  ctor ok')

# ===========================================================================
# 3) openFile 打点
# ===========================================================================
old_open = '''void MainWindow::openFile(const QString &filePath)
{
    QFile file(filePath);'''
new_open = '''void MainWindow::openFile(const QString &filePath)
{
    benchStart();

    QFile file(filePath);'''
if 'benchStart();' not in s:
    assert old_open in s
    s = s.replace(old_open, new_open, 1)

    # 读文件后
    old1 = '''    const QString content = in.readAll();
    file.close();'''
    if old1 not in s:
        old1 = '''    in.setEncoding(QStringConverter::Utf8);
    QString content = in.readAll();
    file.close();'''
        new1 = '''    in.setEncoding(QStringConverter::Utf8);
    QString content = in.readAll();
    file.close();
    benchMark(QStringLiteral("read_file"));'''
    else:
        new1 = '''    const QString content = in.readAll();
    file.close();
    benchMark(QStringLiteral("read_file"));'''
    assert old1 in s
    s = s.replace(old1, new1, 1)

    # setText 之后
    old2 = '''    editor->setText(content);
    editor->setProperty("filePath", filePath);'''
    new2 = '''    editor->setText(content);
    editor->setProperty("filePath", filePath);
    benchMark(QStringLiteral("set_editor_text"));'''
    assert old2 in s
    s = s.replace(old2, new2, 1)

    # 大纲之后
    old3 = '''    // 刷新大纲（打开文件后立即显示其标题结构）
    refreshOutline();'''
    new3 = '''    // 刷新大纲（打开文件后立即显示其标题结构）
    refreshOutline();
    benchMark(QStringLiteral("outline"));'''
    assert old3 in s
    s = s.replace(old3, new3, 1)

    # updatePreview 之后（openFile 末尾）
    old4 = '''    updatePreview();
}

/**
 * 关闭标签页'''
    if old4 not in s:
        # 找 openFile 结尾的 updatePreview();
        idx = s.index('    updatePreview();\n}', s.index('void MainWindow::openFile(const QString &filePath)'))
        s = s[:idx] + '    updatePreview();\n    benchMark(QStringLiteral("open_total"));\n}' + s[idx + len('    updatePreview();\n}'):]
    else:
        s = s.replace(old4, '''    updatePreview();
    benchMark(QStringLiteral("open_total"));
}

/**
 * 关闭标签页''', 1)
    print('  openFile 打点 ok')

# ===========================================================================
# 4) updatePreview 细分打点
# ===========================================================================
old_prev = '''    QString markdown = editor->toPlainText();
    QString html = generateHtml(markdown);
    
    m_preview->setHtml(html);'''
new_prev = '''    QString markdown = editor->toPlainText();
    benchMark(QStringLiteral("preview_toPlainText"));

    QString html = generateHtml(markdown);
    benchMark(QStringLiteral("preview_generateHtml"));

    m_preview->setHtml(html);
    benchMark(QStringLiteral("preview_setHtml"));'''
if 'preview_generateHtml' not in s:
    assert old_prev in s, 'updatePreview 块未匹配'
    s = s.replace(old_prev, new_prev, 1)
    print('  updatePreview 打点 ok')

# ===========================================================================
# 5) bench 辅助函数实现
# ===========================================================================
if 'void MainWindow::benchStart()' not in s:
    IMPL = r'''

// ===========================================================================
// 性能基准打点
// ===========================================================================

/**
 * 开始一次基准计时。
 *
 * 记录的是「累计毫秒」而不是每段耗时，脚本侧再算相邻差值 ——
 * 这样即使某段被跳过（比如文件已打开提前返回），差值仍然正确。
 */
void MainWindow::benchStart()
{
    if (!m_benchMode) {
        return;
    }
    m_benchLog.clear();
    m_benchTimer.start();
    // t=0 基线
    m_benchLog << QStringLiteral("bench_start|0");
}

void MainWindow::benchMark(const QString &phase)
{
    if (!m_benchMode) {
        return;
    }
    const double ms = m_benchTimer.nsecsElapsed() / 1000000.0;
    m_benchLog << QStringLiteral("%1|%2").arg(phase).arg(ms, 0, 'f', 2);
}
'''
    s = s.rstrip() + '\n' + IMPL
    print('  bench 函数 ok')

io.open(pc, 'w', encoding='utf-8', newline='\n').write(s)
print('mainwindow.cpp 已注入打点')
