#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
生成 MainWindow 的 Typora 对齐实现（追加到 mainwindow.cpp 末尾），
并注入 action 创建 + 菜单重构。
"""
import io
import sys

SRC = sys.argv[1]

with io.open(SRC, 'r', encoding='utf-8') as f:
    src = f.read()

# ===========================================================================
# 1) 注入 action 创建（在 m_previewOnlyAction 创建之后）
# ===========================================================================
anchor = '''    connect(m_previewOnlyAction, &QAction::triggered, this, &MainWindow::setPreviewOnlyMode);
'''

new_actions = '''    connect(m_previewOnlyAction, &QAction::triggered, this, &MainWindow::setPreviewOnlyMode);

    // ================= Typora 对齐：文件菜单 =================
    m_newWindowAction = new QAction(tr("新建窗口"), this);
    m_newWindowAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_N));
    connect(m_newWindowAction, &QAction::triggered, this, [this]() {
        MainWindow *w = new MainWindow();
        w->resize(1000, 700);
        w->show();
    });

    m_printAction = new QAction(tr("打印..."), this);
    m_printAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_P));
    connect(m_printAction, &QAction::triggered, this, &MainWindow::showPrintDialog);

    m_preferencesAction = new QAction(tr("偏好设置..."), this);
    m_preferencesAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_Comma));
    connect(m_preferencesAction, &QAction::triggered, this, &MainWindow::showPreferences);

    m_preferencesFileAction = new QAction(tr("偏好设置文件..."), this);
    connect(m_preferencesFileAction, &QAction::triggered, this, [this]() {
        const QString path = QDir(QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation))
                                .filePath("preferences.json");
        QDesktopServices::openUrl(QUrl::fromLocalFile(path));
    });

    // ================= Typora 对齐：编辑菜单 =================
    m_pasteMarkdownAction = new QAction(tr("粘贴为 Markdown"), this);
    m_pasteMarkdownAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_V));
    connect(m_pasteMarkdownAction, &QAction::triggered, this, &MainWindow::pasteAsMarkdown);

    m_selectAllAction = new QAction(tr("全选"), this);
    m_selectAllAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_A));
    connect(m_selectAllAction, &QAction::triggered, this, &MainWindow::selectAll);

    m_copyMarkdownAction = new QAction(tr("复制为 Markdown"), this);
    m_copyMarkdownAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_C));
    connect(m_copyMarkdownAction, &QAction::triggered, this, &MainWindow::copyAsMarkdown);

    m_copyHtmlAction = new QAction(tr("复制为 HTML 代码"), this);
    m_copyHtmlAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_H));
    connect(m_copyHtmlAction, &QAction::triggered, this, &MainWindow::copyAsHtml);

    m_pastePlainTextAction = new QAction(tr("粘贴为纯文本"), this);
    m_pastePlainTextAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_T));
    connect(m_pastePlainTextAction, &QAction::triggered, this, &MainWindow::pasteAsPlainText);

    m_deleteAction = new QAction(tr("删除"), this);
    m_deleteAction->setShortcut(QKeySequence(Qt::Key_Delete));
    connect(m_deleteAction, &QAction::triggered, this, &MainWindow::deleteSelection);

    m_mathToolsAction = new QAction(tr("数学工具"), this);
    m_mathToolsAction->setMenu(new QMenu(this));
    m_mathToolsAction->menu()->addAction(tr("公式块..."));
    m_mathToolsAction->menu()->addAction(tr("内嵌公式..."));
    m_mathToolsAction->menu()->addAction(tr("刷新所有数学公式"));

    // 智能标点子项（Typora「编辑 > 智能标点」为子菜单）
    m_smartQuotesAction = new QAction(tr("智能引号"), this);
    m_smartQuotesAction->setCheckable(true);
    m_smartQuotesAction->setChecked(true);
    connect(m_smartQuotesAction, &QAction::toggled, this, [this](bool on) {
        m_smartPunctuation->setSmartQuotes(on);
    });

    m_smartDashesAction = new QAction(tr("智能破折号"), this);
    m_smartDashesAction->setCheckable(true);
    m_smartDashesAction->setChecked(true);
    connect(m_smartDashesAction, &QAction::toggled, this, [this](bool on) {
        m_smartPunctuation->setSmartDashes(on);
    });

    m_unicodePunctuationAction = new QAction(tr("解析 Markdown 时允许并转化 Unicode 标点"), this);
    m_unicodePunctuationAction->setCheckable(true);
    m_unicodePunctuationAction->setChecked(false);

    // ================= Typora 对齐：段落菜单 =================
    m_heading4Action = new QAction(tr("四级标题"), this);
    m_heading4Action->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_4));
    connect(m_heading4Action, &QAction::triggered, this, [this]() { insertHeading(4); });

    m_heading5Action = new QAction(tr("五级标题"), this);
    m_heading5Action->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_5));
    connect(m_heading5Action, &QAction::triggered, this, [this]() { insertHeading(5); });

    m_heading6Action = new QAction(tr("六级标题"), this);
    m_heading6Action->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_6));
    connect(m_heading6Action, &QAction::triggered, this, [this]() { insertHeading(6); });

    m_paragraphAction = new QAction(tr("段落"), this);
    m_paragraphAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_0));
    connect(m_paragraphAction, &QAction::triggered, this, [this]() { toggleBlockType("paragraph"); });

    m_increaseHeadingAction = new QAction(tr("提升标题级别"), this);
    m_increaseHeadingAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_BracketLeft));
    connect(m_increaseHeadingAction, &QAction::triggered, this, [this]() {
        applyBlockResult(BlockEditor::increaseHeading(currentText(), currentCursorPos()));
    });

    m_decreaseHeadingAction = new QAction(tr("降低标题级别"), this);
    m_decreaseHeadingAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_BracketRight));
    connect(m_decreaseHeadingAction, &QAction::triggered, this, [this]() {
        applyBlockResult(BlockEditor::decreaseHeading(currentText(), currentCursorPos()));
    });

    m_tableBlockAction = new QAction(tr("表格"), this);
    m_tableBlockAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_BracketRight));
    connect(m_tableBlockAction, &QAction::triggered, this, &MainWindow::insertTable);

    m_mathBlockAction = new QAction(tr("公式块"), this);
    m_mathBlockAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_M));
    connect(m_mathBlockAction, &QAction::triggered, this, [this]() { toggleBlockType("math"); });

    m_inlineCodeAction = new QAction(tr("代码"), this);
    m_inlineCodeAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_I));
    connect(m_inlineCodeAction, &QAction::triggered, this, [this]() { toggleInlineFormat("code"); });

    m_taskListAction = new QAction(tr("任务列表"), this);
    m_taskListAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_X));
    connect(m_taskListAction, &QAction::triggered, this, [this]() { toggleBlockType("task"); });

    m_taskStatusAction = new QAction(tr("任务状态"), this);
    m_taskStatusAction->setMenu(new QMenu(this));
    m_taskStatusAction->menu()->addAction(tr("切换任务状态"));
    m_taskStatusAction->menu()->addAction(tr("标记为已完成"));
    m_taskStatusAction->menu()->addAction(tr("标记为未完成"));

    m_indentAction = new QAction(tr("增加缩进"), this);
    m_indentAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_Right));
    connect(m_indentAction, &QAction::triggered, this, &MainWindow::indentList);

    m_outdentAction = new QAction(tr("减少缩进"), this);
    m_outdentAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_Left));
    connect(m_outdentAction, &QAction::triggered, this, &MainWindow::outdentList);

    m_tableRowAction = new QAction(tr("表格行"), this);
    m_tableRowAction->setMenu(new QMenu(this));
    m_tableRowAction->menu()->addAction(tr("上方插入行"));
    m_tableRowAction->menu()->addAction(tr("下方插入行"));
    m_tableRowAction->menu()->addAction(tr("删除行"));

    m_tableColumnAction = new QAction(tr("表格列"), this);
    m_tableColumnAction->setMenu(new QMenu(this));
    m_tableColumnAction->menu()->addAction(tr("左侧插入列"));
    m_tableColumnAction->menu()->addAction(tr("右侧插入列"));
    m_tableColumnAction->menu()->addAction(tr("删除列"));

    m_insertParagraphAboveAction = new QAction(tr("在上方插入段落"), this);
    connect(m_insertParagraphAboveAction, &QAction::triggered, this, &MainWindow::insertParagraphAbove);

    m_insertParagraphBelowAction = new QAction(tr("在下方插入段落"), this);
    connect(m_insertParagraphBelowAction, &QAction::triggered, this, &MainWindow::insertParagraphBelow);

    m_inlineMathAction = new QAction(tr("数学公式"), this);
    connect(m_inlineMathAction, &QAction::triggered, this, &MainWindow::insertInlineMath);

    m_footnoteAction = new QAction(tr("脚注"), this);
    connect(m_footnoteAction, &QAction::triggered, this, &MainWindow::insertFootnote);

    m_tocAction = new QAction(tr("内容目录"), this);
    connect(m_tocAction, &QAction::triggered, this, &MainWindow::insertToc);

    m_yamlFrontMatterAction = new QAction(tr("YAML Front Matter"), this);
    connect(m_yamlFrontMatterAction, &QAction::triggered, this, &MainWindow::insertYamlFrontMatter);

    // ================= Typora 对齐：格式菜单 =================
    m_highlightAction = new QAction(tr("高亮"), this);
    m_highlightAction->setShortcut(QKeySequence(Qt::ALT | Qt::SHIFT | Qt::Key_H));
    connect(m_highlightAction, &QAction::triggered, this, [this]() { toggleInlineFormat("highlight"); });

    m_superscriptAction = new QAction(tr("上标"), this);
    m_superscriptAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_Plus));
    connect(m_superscriptAction, &QAction::triggered, this, [this]() { toggleInlineFormat("superscript"); });

    m_subscriptAction = new QAction(tr("下标"), this);
    m_subscriptAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_Underscore));
    connect(m_subscriptAction, &QAction::triggered, this, [this]() { toggleInlineFormat("subscript"); });

    m_commentAction = new QAction(tr("注释"), this);
    connect(m_commentAction, &QAction::triggered, this, [this]() { toggleInlineFormat("comment"); });

    m_hyperlinkAction = new QAction(tr("超链接"), this);
    m_hyperlinkAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_K));
    connect(m_hyperlinkAction, &QAction::triggered, this, [this]() { toggleInlineFormat("link"); });

    m_createFootnoteAction = new QAction(tr("创建脚注"), this);
    connect(m_createFootnoteAction, &QAction::triggered, this, &MainWindow::insertFootnote);

    m_clearFormatAction = new QAction(tr("清除格式"), this);
    connect(m_clearFormatAction, &QAction::triggered, this, &MainWindow::clearFormat);

    // ================= Typora 对齐：视图菜单 =================
    m_outlineAction = new QAction(tr("大纲"), this);
    m_outlineAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_1));
    connect(m_outlineAction, &QAction::triggered, this, [this]() { setSidebarTab(1); });

    m_docTreeAction = new QAction(tr("文档树"), this);
    m_docTreeAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_2));
    connect(m_docTreeAction, &QAction::triggered, this, [this]() { setSidebarTab(2); });

    m_fileTreeAction = new QAction(tr("文件树"), this);
    m_fileTreeAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_3));
    connect(m_fileTreeAction, &QAction::triggered, this, [this]() { setSidebarTab(0); });

    m_sourceModeAction = new QAction(tr("源代码模式"), this);
    m_sourceModeAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_Slash));
    m_sourceModeAction->setCheckable(true);
    connect(m_sourceModeAction, &QAction::triggered, this, &MainWindow::toggleSourceMode);

    m_typewriterModeAction = new QAction(tr("打字机模式"), this);
    m_typewriterModeAction->setShortcut(QKeySequence(Qt::Key_F10));
    m_typewriterModeAction->setCheckable(true);
    connect(m_typewriterModeAction, &QAction::triggered, this, &MainWindow::setTypewriterMode);

    m_showSourceAction = new QAction(tr("显示源代码"), this);
    m_showSourceAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_D));
    connect(m_showSourceAction, &QAction::triggered, this, [this]() {
        QTextEdit *ed = currentEditor();
        if (!ed) return;
        ed->setPlainText(ed->toPlainText());
    });

    m_sidebarAction = new QAction(tr("侧边栏"), this);
    m_sidebarAction->setShortcut(QKeySequence(Qt::Key_F11));
    connect(m_sidebarAction, &QAction::triggered, this, &MainWindow::toggleSidebar);

    m_alwaysOnTopAction = new QAction(tr("保持窗口在最前端"), this);
    m_alwaysOnTopAction->setCheckable(true);
    connect(m_alwaysOnTopAction, &QAction::triggered, this, &MainWindow::toggleAlwaysOnTop);

    m_actualSizeAction = new QAction(tr("实际大小"), this);
    m_actualSizeAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_9));
    connect(m_actualSizeAction, &QAction::triggered, this, [this]() { updateFontSize(14); });

    m_devToolsAction = new QAction(tr("开发者工具"), this);
    m_devToolsAction->setShortcut(QKeySequence(Qt::SHIFT | Qt::Key_F12));
    connect(m_devToolsAction, &QAction::triggered, this, &MainWindow::showDevTools);
'''

assert anchor in src, 'action 锚点未匹配'
src = src.replace(anchor, new_actions, 1)

with io.open(SRC, 'w', encoding='utf-8', newline='\n') as f:
    f.write(src)

print('  action 创建已注入')
