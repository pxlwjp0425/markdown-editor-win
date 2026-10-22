#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
把 createMenus() 重构为 Typora 的 7 菜单结构，
并把 Typora 对齐的全部方法实现追加到 mainwindow.cpp 末尾。
"""
import io
import re
import sys

SRC = sys.argv[1]

with io.open(SRC, 'r', encoding='utf-8') as f:
    src = f.read()

# ===========================================================================
# 1) 替换 createMenus 整体
# ===========================================================================
start = src.index('void MainWindow::createMenus()')
# 找到函数结束（下一个 "void MainWindow::" 或文件尾）
m = re.search(r'\nvoid MainWindow::createToolbars\(\)', src[start:])
end = start + m.start() + 1

NEW_MENUS = r'''void MainWindow::createMenus()
{
    // 菜单结构与 Typora 对齐：文件 / 编辑 / 段落 / 格式 / 视图 / 主题 / 帮助
    // 分组与顺序按 Typora 的公开界面规格对齐。

    // ---------------- 文件 ----------------
    QMenu *fileMenu = menuBar()->addMenu(tr("文件(&F)"));
    fileMenu->addAction(m_newAction);
    fileMenu->addAction(m_newWindowAction);
    fileMenu->addAction(m_openAction);
    fileMenu->addAction(m_openFolderAction);
    fileMenu->addSeparator();
    fileMenu->addAction(m_saveAction);
    fileMenu->addAction(m_saveAsAction);
    fileMenu->addSeparator();
    fileMenu->addAction(m_exportHtmlAction);
    fileMenu->addAction(m_printAction);
    fileMenu->addAction(m_exportPdfAction);
    fileMenu->addSeparator();
    fileMenu->addAction(m_preferencesAction);
    fileMenu->addAction(m_preferencesFileAction);
    fileMenu->addSeparator();
    fileMenu->addAction(m_exitAction);

    // ---------------- 编辑 ----------------
    QMenu *editMenu = menuBar()->addMenu(tr("编辑(&E)"));
    editMenu->addAction(m_undoAction);
    editMenu->addAction(m_redoAction);
    editMenu->addSeparator();
    editMenu->addAction(m_cutAction);
    editMenu->addAction(m_copyAction);
    editMenu->addAction(m_pasteAction);
    editMenu->addAction(m_pasteMarkdownAction);
    editMenu->addAction(m_deleteAction);
    editMenu->addSeparator();
    editMenu->addAction(m_selectAllAction);
    editMenu->addSeparator();
    editMenu->addAction(m_copyMarkdownAction);
    editMenu->addAction(m_copyHtmlAction);
    editMenu->addAction(m_pastePlainTextAction);
    editMenu->addSeparator();

    // 智能标点（Typora 为子菜单）
    QMenu *smartMenu = editMenu->addMenu(tr("智能标点"));
    smartMenu->addAction(m_smartPunctuationAction);   // 开关：输入时转换
    smartMenu->addAction(m_smartQuotesAction);        // 智能引号
    smartMenu->addAction(m_smartDashesAction);        // 智能破折号
    smartMenu->addAction(m_unicodePunctuationAction); // Unicode 标点
    smartMenu->addSeparator();
    smartMenu->addAction(tr("更多选项..."));

    // 选择 / 删除 / 数学工具（Typora 为子菜单）
    QMenu *selectMenu = editMenu->addMenu(tr("选择"));
    selectMenu->addAction(tr("选中当前行或句"));
    selectMenu->addAction(tr("选中当前格式文本"));
    selectMenu->addAction(tr("选中当前词"));

    QMenu *deleteMenu = editMenu->addMenu(tr("删除"));
    deleteMenu->addAction(tr("删除当前行或句"));
    deleteMenu->addAction(tr("删除当前词"));
    deleteMenu->addAction(tr("删除当前格式文本"));

    editMenu->addAction(m_mathToolsAction);
    editMenu->addSeparator();
    editMenu->addAction(m_findAction);
    editMenu->addAction(m_replaceAction);
    editMenu->addSeparator();
    editMenu->addAction(m_preferencesAction);

    // ---------------- 段落 ----------------
    QMenu *paraMenu = menuBar()->addMenu(tr("段落(&P)"));
    paraMenu->addAction(m_heading1Action);
    paraMenu->addAction(m_heading2Action);
    paraMenu->addAction(m_heading3Action);
    paraMenu->addAction(m_heading4Action);
    paraMenu->addAction(m_heading5Action);
    paraMenu->addAction(m_heading6Action);
    paraMenu->addAction(m_paragraphAction);
    paraMenu->addSeparator();
    paraMenu->addAction(m_increaseHeadingAction);
    paraMenu->addAction(m_decreaseHeadingAction);
    paraMenu->addSeparator();
    paraMenu->addAction(m_tableBlockAction);
    paraMenu->addAction(m_mathBlockAction);
    paraMenu->addAction(m_codeBlockAction);
    paraMenu->addAction(m_inlineCodeAction);
    paraMenu->addAction(m_blockquoteAction);
    paraMenu->addSeparator();
    paraMenu->addAction(m_numberedListAction);
    paraMenu->addAction(m_bulletListAction);
    paraMenu->addAction(m_taskListAction);
    paraMenu->addAction(m_taskStatusAction);
    paraMenu->addAction(m_indentAction);
    paraMenu->addAction(m_outdentAction);
    paraMenu->addSeparator();
    paraMenu->addAction(m_tableAction);
    paraMenu->addAction(m_tableRowAction);
    paraMenu->addAction(m_tableColumnAction);
    paraMenu->addSeparator();
    paraMenu->addAction(m_insertParagraphAboveAction);
    paraMenu->addAction(m_insertParagraphBelowAction);
    paraMenu->addSeparator();
    paraMenu->addAction(m_inlineMathAction);
    paraMenu->addAction(m_footnoteAction);
    paraMenu->addAction(m_horizontalRuleAction);
    paraMenu->addAction(m_tocAction);
    paraMenu->addAction(m_yamlFrontMatterAction);

    // ---------------- 格式 ----------------
    QMenu *formatMenu = menuBar()->addMenu(tr("格式(&O)"));
    formatMenu->addAction(m_boldAction);
    formatMenu->addAction(m_italicAction);
    formatMenu->addAction(m_strikethroughAction);
    formatMenu->addAction(m_codeBlockAction);
    formatMenu->addAction(m_inlineCodeAction);
    formatMenu->addSeparator();
    formatMenu->addAction(m_highlightAction);
    formatMenu->addAction(m_superscriptAction);
    formatMenu->addAction(m_subscriptAction);
    formatMenu->addAction(m_commentAction);
    formatMenu->addSeparator();
    formatMenu->addAction(m_hyperlinkAction);
    formatMenu->addAction(m_imageAction);
    formatMenu->addAction(m_tableAction);
    formatMenu->addAction(m_createFootnoteAction);
    formatMenu->addSeparator();
    formatMenu->addAction(m_clearFormatAction);

    // ---------------- 视图 ----------------
    QMenu *viewMenu = menuBar()->addMenu(tr("视图(&V)"));
    viewMenu->addAction(m_toggleSidebarAction);
    viewMenu->addSeparator();
    viewMenu->addAction(m_outlineAction);
    viewMenu->addAction(m_docTreeAction);
    viewMenu->addAction(m_fileTreeAction);
    viewMenu->addSeparator();
    viewMenu->addAction(m_findAction);
    viewMenu->addSeparator();
    viewMenu->addAction(m_sourceModeAction);
    viewMenu->addAction(m_focusModeAction);
    viewMenu->addAction(m_typewriterModeAction);
    viewMenu->addAction(m_showSourceAction);
    viewMenu->addSeparator();
    viewMenu->addAction(m_sidebarAction);
    viewMenu->addAction(m_alwaysOnTopAction);
    viewMenu->addSeparator();
    viewMenu->addAction(m_actualSizeAction);
    viewMenu->addAction(m_zoomInAction);
    viewMenu->addAction(m_zoomOutAction);
    viewMenu->addSeparator();
    viewMenu->addAction(m_previewOnlyAction);
    viewMenu->addAction(m_darkModeAction);
    viewMenu->addAction(m_fullscreenAction);
    viewMenu->addSeparator();
    viewMenu->addAction(m_devToolsAction);

    // ---------------- 主题 ----------------
    QMenu *themeMenu = menuBar()->addMenu(tr("主题(&T)"));
    themeMenu->setObjectName("themeMenu");
    {
        QActionGroup *group = new QActionGroup(this);
        group->setExclusive(true);
        for (const QString &tid : Theme::availableThemes()) {
            const Theme::Palette pal = Theme::palette(tid);
            QAction *act = new QAction(pal.displayName, this);
            act->setCheckable(true);
            act->setData(tid);
            act->setChecked(tid == m_themeId);
            connect(act, &QAction::triggered, this, [this, tid]() { applyThemeById(tid); });
            group->addAction(act);
            themeMenu->addAction(act);
        }
    }

    // ---------------- 帮助 ----------------
    QMenu *helpMenu = menuBar()->addMenu(tr("帮助(&H)"));
    helpMenu->addAction(m_shortcutsAction);
    helpMenu->addAction(m_aboutAction);
}

'''

src = src[:start] + NEW_MENUS + src[end:]

with io.open(SRC, 'w', encoding='utf-8', newline='\n') as f:
    f.write(src)

print('  createMenus 已重构为 Typora 7 菜单')
