#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
1) 文档树刷新：标签页增删改时同步
2) 打字机模式：光标移动时自动居中
"""
import io

p = 'src/mainwindow.cpp'
s = io.open(p, encoding='utf-8').read()

# ---------------------------------------------------------------------------
# 1) 文档树刷新函数
# ---------------------------------------------------------------------------
IMPL = r'''

/**
 * 刷新「文档树」页签：列出当前打开的所有文档。
 *
 * 对应 Typora 的「文档树」面板（视图 > 文档树 Ctrl+Shift+2）。
 */
void MainWindow::refreshDocTree()
{
    if (!m_docTree) {
        return;
    }
    m_docTree->clear();

    for (int i = 0; i < m_editorTabs->count(); ++i) {
        QTextEdit *editor = qobject_cast<QTextEdit*>(m_editorTabs->widget(i));
        if (!editor) {
            continue;
        }
        const QString path = editor->property("filePath").toString();
        QString label = m_editorTabs->tabText(i);
        if (label.endsWith("*")) {
            label.chop(1);
        }
        if (path.isEmpty()) {
            label = tr("未命名 %1").arg(i + 1);
        } else {
            label = QFileInfo(path).fileName();
        }

        QTreeWidgetItem *item = new QTreeWidgetItem(m_docTree);
        item->setText(0, label);
        item->setData(0, Qt::UserRole, i);
        item->setToolTip(0, path.isEmpty() ? label : path);

        // 当前文档高亮
        if (i == m_editorTabs->currentIndex()) {
            item->setForeground(0, Theme::accentColor(m_themeId));
        }
    }
    m_docTree->expandAll();
}
'''

s = s.rstrip() + '\n' + IMPL

# ---------------------------------------------------------------------------
# 2) 在 onTabChanged 中刷新文档树
# ---------------------------------------------------------------------------
old = '''    // 大纲随当前标签页切换（Typora 的 Outline 行为）
    refreshOutline();
    
    updatePreview();
    updateStatusBar();
}'''
new = '''    // 大纲随当前标签页切换（Typora 的 Outline 行为）
    refreshOutline();
    // 文档树同步
    refreshDocTree();
    
    updatePreview();
    updateStatusBar();
}'''
assert old in s, 'onTabChanged 未匹配'
s = s.replace(old, new, 1)

# ---------------------------------------------------------------------------
# 3) 打字机模式：光标移动时居中
# ---------------------------------------------------------------------------
old2 = '''void MainWindow::onEditorCursorMoved()
{
    if (!m_outlinePanel) {
        return;
    }'''
new2 = '''void MainWindow::onEditorCursorMoved()
{
    // 打字机模式：光标始终垂直居中
    if (m_typewriterMode) {
        centerCursorVertically();
    }
    if (!m_outlinePanel) {
        return;
    }'''
assert old2 in s, 'onEditorCursorMoved 未匹配'
s = s.replace(old2, new2, 1)

io.open(p, 'w', encoding='utf-8', newline='\n').write(s)
print('  文档树刷新 + 打字机滚动已接入')

# ---------------------------------------------------------------------------
# 4) 头文件加 refreshDocTree 声明
# ---------------------------------------------------------------------------
ph = 'src/mainwindow.h'
h = io.open(ph, encoding='utf-8').read()
if 'void refreshDocTree();' not in h:
    h = h.replace('    void refreshOutline();',
                  '    void refreshOutline();\n    void refreshDocTree();', 1)
    io.open(ph, 'w', encoding='utf-8', newline='\n').write(h)
    print('  mainwindow.h 已加 refreshDocTree')
