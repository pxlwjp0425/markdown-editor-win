#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""给侧边栏加「文档树」页签（对应 Typora 的 Documents 面板）。"""
import io

p = 'src/mainwindow.cpp'
s = io.open(p, encoding='utf-8').read()

old = '''    // 大纲面板（对应 Typora 的 Outline）
    m_outlinePanel = new OutlinePanel(m_sidebarTabs);
    m_sidebarTabs->addTab(m_outlinePanel, tr("大纲"));'''

new = '''    // 大纲面板（对应 Typora 的 Outline）
    m_outlinePanel = new OutlinePanel(m_sidebarTabs);
    m_sidebarTabs->addTab(m_outlinePanel, tr("大纲"));
    
    // 文档树（对应 Typora 的「文档树」）：列出已打开的文档
    m_docTree = new QTreeWidget(m_sidebarTabs);
    m_docTree->setHeaderHidden(true);
    m_docTree->setIndentation(14);
    m_docTree->setAnimated(true);
    connect(m_docTree, &QTreeWidget::itemClicked, this, [this](QTreeWidgetItem *item, int) {
        const int idx = item->data(0, Qt::UserRole).toInt();
        if (idx >= 0 && idx < m_editorTabs->count()) {
            m_editorTabs->setCurrentIndex(idx);
        }
    });
    m_sidebarTabs->addTab(m_docTree, tr("文档树"));'''

assert old in s, '大纲面板块未匹配'
s = s.replace(old, new, 1)

io.open(p, 'w', encoding='utf-8', newline='\n').write(s)
print('  文档树页签已创建')
