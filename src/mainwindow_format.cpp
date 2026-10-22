/*
 * MainWindow —— Markdown 格式命令。
 *
 * 「格式」菜单与「段落」菜单背后的全部动作：行内格式、块级格式、
 * 插入各类内容、复制粘贴转换。逻辑上都是纯文本变换，
 * 与界面无关的部分都在 InlineFormatter / BlockEditor 里。
 */

#include "mainwindow.h"
#include <QApplication>
#include <QClipboard>
#include <QMimeData>
#include <QFileDialog>

void MainWindow::insertBold()
{
    wrapSelection("**", "**");
}

void MainWindow::insertItalic()
{
    wrapSelection("*", "*");
}

void MainWindow::insertStrikethrough()
{
    wrapSelection("~~", "~~");
}

void MainWindow::insertHeading1()
{
    insertAtLineStart("# ");
}

void MainWindow::insertHeading2()
{
    insertAtLineStart("## ");
}

void MainWindow::insertHeading3()
{
    insertAtLineStart("### ");
}

void MainWindow::insertCode()
{
    wrapSelection("`", "`");
}

void MainWindow::insertCodeBlock()
{
    wrapSelection("```\n", "\n```");
}

void MainWindow::insertLink()
{
    QString selectedText = currentEditor()->textCursor().selectedText();
    QString linkText = selectedText.isEmpty() ? "链接文本" : selectedText;
    QString replacement = QString("[%1](url)").arg(linkText);
    insertText(replacement);
}

void MainWindow::insertImage()
{
    QString fileName = QFileDialog::getOpenFileName(
        this,
        tr("选择图片"),
        m_currentFolder,
        "Images (*.png *.jpg *.jpeg *.gif *.bmp)"
    );
    
    if (!fileName.isEmpty()) {
        QFileInfo fileInfo(fileName);
        // Markdown 链接使用 URL 风格路径，Windows 的反斜杠会破坏渲染
        QString urlPath = QDir::toNativeSeparators(fileName);
        urlPath.replace('\\', '/');
        QString markdown = QString("![%1](%2)").arg(fileInfo.fileName()).arg(urlPath);
        insertText(markdown);
    }
}

void MainWindow::insertTable()
{
    QString table = 
        "| 列1 | 列2 | 列3 |\n"
        "|------|------|------|\n"
        "| 内容 | 内容 | 内容 |\n"
        "| 内容 | 内容 | 内容 |";
    insertText(table);
}

void MainWindow::insertListBullet()
{
    insertAtLineStart("- ");
}

void MainWindow::insertListNumbered()
{
    insertAtLineStart("1. ");
}

void MainWindow::insertBlockquote()
{
    insertAtLineStart("> ");
}

void MainWindow::insertHorizontalRule()
{
    insertText("\n---\n");
}

void MainWindow::wrapSelection(const QString &prefix, const QString &suffix)
{
    QPlainTextEdit *editor = currentEditor();
    if (!editor) return;
    
    QTextCursor cursor = editor->textCursor();
    QString selectedText = cursor.selectedText();
    
    cursor.insertText(prefix + selectedText + suffix);
    editor->setTextCursor(cursor);
}

void MainWindow::insertText(const QString &text)
{
    QPlainTextEdit *editor = currentEditor();
    if (!editor) return;
    
    QTextCursor cursor = editor->textCursor();
    cursor.insertText(text);
    editor->setTextCursor(cursor);
}

void MainWindow::insertAtLineStart(const QString &prefix)
{
    QPlainTextEdit *editor = currentEditor();
    if (!editor) return;
    
    QTextCursor cursor = editor->textCursor();
    cursor.movePosition(QTextCursor::StartOfLine);
    cursor.insertText(prefix);
    editor->setTextCursor(cursor);
}

void MainWindow::toggleInlineFormat(const QString &id)
{
    QPlainTextEdit *editor = currentEditor();
    if (!editor || !m_inlineFormatter) {
        return;
    }
    const QTextCursor cursor = editor->textCursor();
    const QString text = editor->toPlainText();

    QString outText;
    int outStart = 0, outEnd = 0;

    // 链接需要地址占位
    QString placeholder;
    if (id == QLatin1String("link")) {
        placeholder = tr("链接文字");
    } else if (id == QLatin1String("math")) {
        placeholder = QStringLiteral("E=mc^2");
    } else if (id == QLatin1String("code")) {
        placeholder = tr("代码");
    } else {
        placeholder = tr("文本");
    }

    if (!m_inlineFormatter->toggle(id, text, cursor.selectionStart(), cursor.selectionEnd(),
                                   outText, outStart, outEnd, placeholder)) {
        return;
    }

    editor->setPlainText(outText);
    QTextCursor nc(editor->document());
    const int maxPos = editor->document()->characterCount() - 1;
    nc.setPosition(qBound(0, outStart, maxPos));
    nc.setPosition(qBound(0, outEnd, maxPos), QTextCursor::KeepAnchor);
    editor->setTextCursor(nc);
    editor->setFocus();
}

void MainWindow::insertHeading(int level)
{
    QPlainTextEdit *editor = currentEditor();
    if (!editor) {
        return;
    }
    const QTextCursor cursor = editor->textCursor();
    const int pos = cursor.selectionStart();

    // 若当前行已是标题，替换其级别；否则插入新标题行
    const QString text = editor->toPlainText();
    int lineStart = text.lastIndexOf('\n', qMax(0, pos - 1)) + 1;
    int lineEnd = text.indexOf('\n', pos);
    if (lineEnd < 0) {
        lineEnd = text.length();
    }
    const QString line = text.mid(lineStart, lineEnd - lineStart);
    const int curLevel = BlockEditor::headingLevel(line);

    QString newLine;
    if (curLevel > 0) {
        newLine = QString(level, '#') + line.mid(curLevel);
    } else {
        const QString body = line.trimmed();
        newLine = QString(level, '#') + QLatin1Char(' ') + body;
    }

    editor->setPlainText(text.left(lineStart) + newLine + text.mid(lineEnd));
    QTextCursor nc(editor->document());
    nc.setPosition(lineStart + newLine.length());
    editor->setTextCursor(nc);
    editor->setFocus();
}

void MainWindow::toggleBlockType(const QString &type)
{
    const BlockEditor::Result r =
        BlockEditor::toggleBlock(currentText(), currentCursorPos(), type);
    applyBlockResult(r);
}

void MainWindow::insertFootnote()
{
    applyBlockResult(BlockEditor::insertBlock(currentText(), currentCursorPos(), "footnote"));
}

void MainWindow::insertToc()
{
    applyBlockResult(BlockEditor::insertBlock(currentText(), currentCursorPos(), "toc"));
}

void MainWindow::insertYamlFrontMatter()
{
    applyBlockResult(BlockEditor::insertBlock(currentText(), currentCursorPos(), "yaml"));
}

void MainWindow::insertInlineMath()
{
    toggleInlineFormat("math");
}

void MainWindow::insertParagraphAbove()
{
    applyBlockResult(BlockEditor::insertParagraph(currentText(), currentCursorPos(), "above"));
}

void MainWindow::insertParagraphBelow()
{
    applyBlockResult(BlockEditor::insertParagraph(currentText(), currentCursorPos(), "below"));
}

void MainWindow::toggleTaskStatus()
{
    QPlainTextEdit *editor = currentEditor();
    if (!editor) {
        return;
    }
    const QString text = editor->toPlainText();
    const int pos = currentCursorPos();

    int lineStart = text.lastIndexOf('\n', qMax(0, pos - 1)) + 1;
    int lineEnd = text.indexOf('\n', pos);
    if (lineEnd < 0) {
        lineEnd = text.length();
    }
    QString line = text.mid(lineStart, lineEnd - lineStart);

    QRegularExpression rx("^(\\s*[-*+]\\s+\\[)([ xX])(\\])");
    QRegularExpressionMatch m = rx.match(line);
    if (!m.hasMatch()) {
        return;
    }
    const bool checked = m.captured(2).toLower() == "x";
    const QString mark = checked ? " " : "x";
    line.replace(rx, QStringLiteral("\\1%1\\3").arg(mark));

    editor->setPlainText(text.left(lineStart) + line + text.mid(lineEnd));
    QTextCursor nc(editor->document());
    nc.setPosition(lineStart + line.length());
    editor->setTextCursor(nc);
    editor->setFocus();
}

void MainWindow::indentList()
{
    QPlainTextEdit *editor = currentEditor();
    if (!editor) {
        return;
    }
    const QString text = editor->toPlainText();
    const int pos = currentCursorPos();

    int lineStart = text.lastIndexOf('\n', qMax(0, pos - 1)) + 1;
    int lineEnd = text.indexOf('\n', pos);
    if (lineEnd < 0) {
        lineEnd = text.length();
    }
    QString line = text.mid(lineStart, lineEnd - lineStart);

    // 列表/任务项缩进两个空格
    QRegularExpression rx("^(\\s*)([-*+]|\\d+[.)]\\s)");
    QRegularExpressionMatch m = rx.match(line);
    if (m.hasMatch()) {
        line = m.captured(1) + QStringLiteral("  ") + line.mid(m.capturedLength());
    } else {
        line = QStringLiteral("  ") + line;
    }

    editor->setPlainText(text.left(lineStart) + line + text.mid(lineEnd));
    QTextCursor nc(editor->document());
    nc.setPosition(lineStart + line.length());
    editor->setTextCursor(nc);
    editor->setFocus();
}

void MainWindow::outdentList()
{
    QPlainTextEdit *editor = currentEditor();
    if (!editor) {
        return;
    }
    const QString text = editor->toPlainText();
    const int pos = currentCursorPos();

    int lineStart = text.lastIndexOf('\n', qMax(0, pos - 1)) + 1;
    int lineEnd = text.indexOf('\n', pos);
    if (lineEnd < 0) {
        lineEnd = text.length();
    }
    QString line = text.mid(lineStart, lineEnd - lineStart);

    // 去掉最多两个前导空格
    int removed = 0;
    while (removed < 2 && removed < line.length() && line.at(removed) == ' ') {
        ++removed;
    }
    line = line.mid(removed);

    editor->setPlainText(text.left(lineStart) + line + text.mid(lineEnd));
    QTextCursor nc(editor->document());
    nc.setPosition(lineStart + line.length());
    editor->setTextCursor(nc);
    editor->setFocus();
}

void MainWindow::insertTableRow(bool above)
{
    applyBlockResult(BlockEditor::insertTableRow(currentText(), currentCursorPos(),
                                                 above ? "above" : "below"));
}

void MainWindow::deleteTableRow()
{
    applyBlockResult(BlockEditor::deleteTableRow(currentText(), currentCursorPos()));
}

void MainWindow::insertTableColumn(bool before)
{
    applyBlockResult(BlockEditor::insertTableColumn(currentText(), currentCursorPos(),
                                                    before ? "before" : "after"));
}

void MainWindow::deleteTableColumn()
{
    applyBlockResult(BlockEditor::deleteTableColumn(currentText(), currentCursorPos()));
}

void MainWindow::clearFormat()
{
    QPlainTextEdit *editor = currentEditor();
    if (!editor) {
        return;
    }
    const QTextCursor cursor = editor->textCursor();
    if (cursor.hasSelection()) {
        // 直接把选区内容替换为去掉所有行内标记的纯文本
        const QString selected = cursor.selectedText().replace(QChar(0x2029), '\n');
        QString plain = selected;
        plain.remove(QRegularExpression("\\*\\*|~~|==|`|\\[\\]|\\(\\)|\\^|~|<!--|-->"));
        editor->insertPlainText(plain);
        return;
    }
    // 无选区：清除当前行所有行内标记
    const QString text = editor->toPlainText();
    const int pos = cursor.position();
    int lineStart = text.lastIndexOf('\n', qMax(0, pos - 1)) + 1;
    int lineEnd = text.indexOf('\n', pos);
    if (lineEnd < 0) {
        lineEnd = text.length();
    }
    QString line = text.mid(lineStart, lineEnd - lineStart);
    line.remove(QRegularExpression("\\*\\*|~~|==|`|\\[\\]|\\(\\)|\\^|~|<!--|-->"));
    editor->setPlainText(text.left(lineStart) + line + text.mid(lineEnd));
    QTextCursor nc(editor->document());
    nc.setPosition(lineStart + line.length());
    editor->setTextCursor(nc);
}

void MainWindow::copyAsMarkdown()
{
    QPlainTextEdit *editor = currentEditor();
    if (!editor) {
        return;
    }
    QClipboard *clip = QApplication::clipboard();
    clip->setText(editor->toPlainText());
    statusBar()->showMessage(tr("已复制 Markdown 源码"), 1500);
}

void MainWindow::copyAsHtml()
{
    QPlainTextEdit *editor = currentEditor();
    if (!editor) {
        return;
    }
    QClipboard *clip = QApplication::clipboard();
    clip->setText(generateHtml(editor->toPlainText()));
    statusBar()->showMessage(tr("已复制 HTML 代码"), 1500);
}

void MainWindow::pasteAsPlainText()
{
    QClipboard *clip = QApplication::clipboard();
    QPlainTextEdit *editor = currentEditor();
    if (!editor) {
        return;
    }
    editor->insertPlainText(clip->text());
}

void MainWindow::pasteAsMarkdown()
{
    QClipboard *clip = QApplication::clipboard();
    QPlainTextEdit *editor = currentEditor();
    if (!editor) {
        return;
    }
    // 剪贴板里是 HTML 就取其纯文本，否则直接用 text
    const QString html = clip->mimeData()->hasHtml() ? clip->mimeData()->html() : QString();
    if (!html.isEmpty()) {
        QTextDocument doc;
        doc.setHtml(html);
        editor->insertPlainText(doc.toPlainText());
    } else {
        editor->insertPlainText(clip->text());
    }
}

void MainWindow::deleteSelection()
{
    QPlainTextEdit *editor = currentEditor();
    if (!editor) {
        return;
    }
    QTextCursor cursor = editor->textCursor();
    if (cursor.hasSelection()) {
        cursor.removeSelectedText();
    } else {
        cursor.deleteChar();
    }
    editor->setTextCursor(cursor);
}
