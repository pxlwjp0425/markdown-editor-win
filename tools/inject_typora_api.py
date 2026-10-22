#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
在 mainwindow.h 中注入 Typora 对齐所需的新增成员与槽。
"""
import io
import sys

path = sys.argv[1]
with io.open(path, 'r', encoding='utf-8') as f:
    src = f.read()

# ---------------------------------------------------------------------------
# 1) include
# ---------------------------------------------------------------------------
old_inc = '#include "theme.h"'
new_inc = '#include "theme.h"\n#include "inlineformatter.h"\n#include "blockeditor.h"'
if '#include "inlineformatter.h"' not in src:
    assert old_inc in src
    src = src.replace(old_inc, new_inc, 1)

# ---------------------------------------------------------------------------
# 2) 新的 action 成员
# ---------------------------------------------------------------------------
old_actions = '''    QAction *m_toggleOutlineAction;
    QAction *m_toggleSourceModeAction;
    QAction *m_smartPunctuationAction;
    QAction *m_previewOnlyAction;'''

new_actions = '''    QAction *m_toggleOutlineAction;
    QAction *m_toggleSourceModeAction;
    QAction *m_smartPunctuationAction;
    QAction *m_previewOnlyAction;

    // ---- Typora 对齐新增：文件 ----
    QAction *m_newWindowAction;
    QAction *m_printAction;
    QAction *m_preferencesAction;
    QAction *m_preferencesFileAction;

    // ---- Typora 对齐新增：编辑 ----
    QAction *m_pasteMarkdownAction;
    QAction *m_selectAllAction;
    QAction *m_copyMarkdownAction;
    QAction *m_copyHtmlAction;
    QAction *m_pastePlainTextAction;
    QAction *m_deleteAction;
    QAction *m_mathToolsAction;
    QAction *m_smartQuotesAction;
    QAction *m_smartDashesAction;
    QAction *m_unicodePunctuationAction;

    // ---- Typora 对齐新增：段落 ----
    QAction *m_heading4Action;
    QAction *m_heading5Action;
    QAction *m_heading6Action;
    QAction *m_paragraphAction;
    QAction *m_increaseHeadingAction;
    QAction *m_decreaseHeadingAction;
    QAction *m_tableBlockAction;
    QAction *m_mathBlockAction;
    QAction *m_inlineCodeAction;
    QAction *m_taskListAction;
    QAction *m_taskStatusAction;
    QAction *m_indentAction;
    QAction *m_outdentAction;
    QAction *m_tableRowAction;
    QAction *m_tableColumnAction;
    QAction *m_insertParagraphAboveAction;
    QAction *m_insertParagraphBelowAction;
    QAction *m_inlineMathAction;
    QAction *m_footnoteAction;
    QAction *m_tocAction;
    QAction *m_yamlFrontMatterAction;

    // ---- Typora 对齐新增：格式 ----
    QAction *m_highlightAction;
    QAction *m_superscriptAction;
    QAction *m_subscriptAction;
    QAction *m_commentAction;
    QAction *m_hyperlinkAction;
    QAction *m_createFootnoteAction;
    QAction *m_clearFormatAction;

    // ---- Typora 对齐新增：视图 ----
    QAction *m_outlineAction;
    QAction *m_docTreeAction;
    QAction *m_fileTreeAction;
    QAction *m_sourceModeAction;
    QAction *m_typewriterModeAction;
    QAction *m_showSourceAction;
    QAction *m_sidebarAction;
    QAction *m_alwaysOnTopAction;
    QAction *m_actualSizeAction;
    QAction *m_devToolsAction;'''

if 'm_newWindowAction' not in src:
    assert old_actions in src, 'action 成员块未匹配'
    src = src.replace(old_actions, new_actions, 1)

# ---------------------------------------------------------------------------
# 3) 新的槽
# ---------------------------------------------------------------------------
old_slots = '''    // 格式操作
    void insertBold();
    void insertItalic();
    void insertStrikethrough();
    void insertHeading1();
    void insertHeading2();
    void insertHeading3();
    void insertCode();
    void insertCodeBlock();
    void insertLink();
    void insertImage();
    void insertTable();
    void insertListBullet();
    void insertListNumbered();
    void insertBlockquote();
    void insertHorizontalRule();'''

new_slots = '''    // 格式操作
    void insertBold();
    void insertItalic();
    void insertStrikethrough();
    void insertHeading1();
    void insertHeading2();
    void insertHeading3();
    void insertCode();
    void insertCodeBlock();
    void insertLink();
    void insertImage();
    void insertTable();
    void insertListBullet();
    void insertListNumbered();
    void insertBlockquote();
    void insertHorizontalRule();

    // ---- Typora 对齐：通用格式切换 ----
    void toggleInlineFormat(const QString &id);
    void insertHeading(int level);
    void toggleBlockType(const QString &type);
    void insertFootnote();
    void insertToc();
    void insertYamlFrontMatter();
    void insertInlineMath();
    void insertParagraphAbove();
    void insertParagraphBelow();
    void toggleTaskStatus();
    void indentList();
    void outdentList();
    void insertTableRow(bool above);
    void deleteTableRow();
    void insertTableColumn(bool before);
    void deleteTableColumn();
    void clearFormat();
    void setTypewriterMode(bool enabled);
    void setSidebarTab(int index);
    void copyAsMarkdown();
    void copyAsHtml();
    void pasteAsPlainText();
    void pasteAsMarkdown();
    void deleteSelection();
    void showPreferences();
    void showPrintDialog();
    void toggleAlwaysOnTop(bool enabled);
    void showDevTools();'''

if 'void toggleInlineFormat' not in src:
    assert old_slots in src, '槽块未匹配'
    src = src.replace(old_slots, new_slots, 1)

# ---------------------------------------------------------------------------
# 4) 新的私有成员
# ---------------------------------------------------------------------------
old_members = '''    // 计时器
    QTimer *m_autoSaveTimer;'''
new_members = '''    // 行内格式 / 块级编辑引擎
    InlineFormatter *m_inlineFormatter;
    BlockEditor *m_blockEditor;

    // 打字机模式
    bool m_typewriterMode;

    // 侧边栏当前页签（0=文件树 1=大纲 2=文档树）
    int m_sidebarTabIndex;

    // 计时器
    QTimer *m_autoSaveTimer;'''
if 'm_inlineFormatter' not in src:
    assert old_members in src, '成员块未匹配'
    src = src.replace(old_members, new_members, 1)

with io.open(path, 'w', encoding='utf-8', newline='\n') as f:
    f.write(src)

print('mainwindow.h 已注入')
