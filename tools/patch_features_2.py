#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
阶段 2：实现 Typora 风格功能的实际逻辑。
"""
import io
import sys

SRC = sys.argv[1]

with io.open(SRC, 'r', encoding='utf-8') as f:
    src = f.read()

# ===========================================================================
# 1) 在类外追加新方法的实现
# ===========================================================================
IMPL = r'''

// ===========================================================================
// Typora 风格功能实现
// ===========================================================================

/**
 * 大纲面板显隐切换（对应 Typora 的「视图 > 大纲」）。
 */
void MainWindow::toggleOutline()
{
    if (!m_sidebarTabs) {
        return;
    }
    const int idx = m_sidebarTabs->indexOf(m_outlinePanel);
    if (idx >= 0) {
        m_sidebarTabs->setCurrentIndex(idx);
    }
    m_toggleOutlineAction->setChecked(true);
}

/**
 * 源码模式切换（对应 Typora 的 Source Code Mode）。
 *
 * Typora 的源码模式隐藏所有富文本渲染与预览，只显示原始 Markdown 源码。
 * 本实现：隐藏预览区、把编辑器切为纯文本显示、关闭语法高亮。
 */
void MainWindow::toggleSourceMode()
{
    m_sourceMode = !m_sourceMode;
    m_toggleSourceModeAction->setChecked(m_sourceMode);

    m_preview->setVisible(!m_sourceMode && m_previewVisible);

    // 源码模式下停用语法高亮（高亮会干扰阅读原始标记）
    for (int i = 0; i < m_editorTabs->count(); ++i) {
        QTextEdit *editor = qobject_cast<QTextEdit*>(m_editorTabs->widget(i));
        if (!editor) continue;
        MarkdownHighlighter *hl = editor->findChild<MarkdownHighlighter*>();
        if (hl) {
            hl->setDocument(hl->document());
        }
    }

    statusBar()->showMessage(m_sourceMode ? tr("源码模式已启用") : tr("源码模式已关闭"), 2000);
}

/**
 * 应用主题（对应 Typora 的「视图 > 主题」）。
 */
void MainWindow::applyThemeById(const QString &themeId)
{
    m_themeId = themeId;
    applyCurrentTheme();
    saveSettings();
}

/**
 * 把当前主题应用到编辑器、预览区、大纲面板。
 */
void MainWindow::applyCurrentTheme()
{
    const Theme::Palette pal = Theme::palette(m_themeId);

    // 预览区 CSS
    const QString css = Theme::previewCss(m_themeId);
    m_preview->document()->setDefaultStyleSheet(css);

    // 所有编辑器
    for (int i = 0; i < m_editorTabs->count(); ++i) {
        QTextEdit *editor = qobject_cast<QTextEdit*>(m_editorTabs->widget(i));
        if (editor) {
            editor->setStyleSheet(Theme::editorStyleSheet(m_themeId));
        }
    }

    // 大纲面板
    if (m_outlinePanel) {
        m_outlinePanel->applyTheme(m_themeId);
    }

    // 深色模式与主题联动：深色主题自动开启深色模式
    if (pal.dark && !m_darkMode) {
        setDarkMode(true);
    }

    updatePreview();
}

/**
 * 大纲点击 -> 跳转到编辑器对应行。
 */
void MainWindow::onOutlineNavigate(int line)
{
    QTextEdit *editor = currentEditor();
    if (!editor) {
        return;
    }

    // 定位到该行
    QTextCursor cursor = editor->textCursor();
    cursor.movePosition(QTextCursor::Start);
    cursor.movePosition(QTextCursor::Down, QTextCursor::MoveAnchor, line);
    editor->setTextCursor(cursor);
    editor->setFocus();
    editor->ensureCursorVisible();
}

/**
 * 预览区锚点点击 -> 分派处理。
 *
 * 两种锚点：
 *   #<heading-anchor>  —— [TOC] 目录项，跳转到对应标题
 *   #md-task:<line>    —— 任务列表勾选框，切换 [ ] / [x]
 */
void MainWindow::onPreviewAnchorClicked(const QUrl &url)
{
    if (!url.isValid() || !url.isFragment()) {
        return;
    }
    const QString fragment = url.fragment();
    if (fragment.isEmpty()) {
        return;
    }

    // 任务列表勾选框
    if (fragment.startsWith(QLatin1String("md-task:"))) {
        bool ok = false;
        const int line = fragment.mid(8).toInt(&ok);
        if (ok) {
            toggleTaskAtLine(line);
        }
        return;
    }

    // [TOC] 目录项：跳转到对应标题
    QTextEdit *editor = currentEditor();
    if (!editor) {
        return;
    }

    const QStringList lines = editor->toPlainText().split('\n');
    QMap<QString, int> used;
    for (int i = 0; i < lines.size(); ++i) {
        QRegularExpression headingRx("^(#{1,6})\\s+(.*)$");
        QRegularExpressionMatch m = headingRx.match(lines.at(i));
        if (!m.hasMatch()) {
            continue;
        }
        QString text = m.captured(2).trimmed();
        text.replace(QRegularExpression("\\s+#+$"), QString());
        text = text.trimmed();

        const QString base = anchorBase(text);
        const QString anchor = uniqueAnchor(base, used);
        used[base] = used.value(base, 0) + 1;

        if (anchor == fragment) {
            onOutlineNavigate(i);
            return;
        }
    }
}

/**
 * 切换指定源码行的任务状态（[ ] <-> [x]）。
 */
void MainWindow::toggleTaskAtLine(int line)
{
    QTextEdit *editor = currentEditor();
    if (!editor) {
        return;
    }

    QStringList lines = editor->toPlainText().split('\n');
    if (line < 0 || line >= lines.size()) {
        return;
    }

    QRegularExpression taskRx("^(\\s*)([-*+])\\s+\\[([ xX])\\](\\s+)(.*)$");
    QRegularExpressionMatch m = taskRx.match(lines.at(line));
    if (!m.hasMatch()) {
        return;
    }

    const bool wasChecked = m.captured(3).toLower() == "x";
    const QString newMark = wasChecked ? QStringLiteral(" ") : QStringLiteral("x");

    QString updated = lines.at(line);
    updated.replace(taskRx, QStringLiteral("\\1\\2 [%4]\\5%6")
                        .arg(newMark)
                        .arg(m.captured(4))
                        .arg(m.captured(5)));
    lines[line] = updated;

    // 记录光标位置，替换后恢复
    const int oldPos = editor->textCursor().position();

    editor->setPlainText(lines.join('\n'));

    // 恢复光标到同一行的同一列
    int pos = 0;
    for (int k = 0; k < line; ++k) {
        pos += lines.at(k).length() + 1;
    }
    pos += qMin(oldPos - 0, lines.at(line).length());
    QTextCursor restore(editor->document());
    restore.setPosition(qBound(0, pos, editor->document()->characterCount() - 1));
    editor->setTextCursor(restore);

    statusBar()->showMessage(wasChecked ? tr("任务已标记为未完成")
                                        : tr("任务已标记为完成"), 1500);
}

/**
 * 编辑器光标移动 -> 高亮大纲中的当前章节。
 */
void MainWindow::onEditorCursorMoved()
{
    if (!m_outlinePanel) {
        return;
    }
    QTextEdit *editor = qobject_cast<QTextEdit*>(sender());
    if (!editor) {
        return;
    }
    const int line = editor->textCursor().blockNumber();
    m_outlinePanel->setCurrentLine(line);
}

/**
 * 刷新大纲（内容变化时调用）。
 */
void MainWindow::refreshOutline()
{
    if (!m_outlinePanel) {
        return;
    }
    QTextEdit *editor = currentEditor();
    if (!editor) {
        m_outlinePanel->clear();
        return;
    }
    m_outlinePanel->updateOutline(editor->toPlainText());
}

/**
 * 智能标点开关。
 */
void MainWindow::onSmartPunctuationToggled(bool enabled)
{
    if (m_smartPunctuation) {
        m_smartPunctuation->setEnabled(enabled);
    }
    statusBar()->showMessage(enabled ? tr("智能标点已开启") : tr("智能标点已关闭"), 2000);
}
'''

# 追加到文件末尾之前（放到最后一个 } 之后）
src = src.rstrip() + '\n' + IMPL

with io.open(SRC, 'w', encoding='utf-8', newline='\n') as f:
    f.write(src)

print('  新方法实现已追加')
