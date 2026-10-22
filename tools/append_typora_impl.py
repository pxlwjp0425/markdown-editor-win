#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""追加 Typora 对齐的全部方法实现到 mainwindow.cpp。"""
import io
import sys

SRC = sys.argv[1]

with io.open(SRC, 'r', encoding='utf-8') as f:
    src = f.read()

IMPL = r'''

// ===========================================================================
// Typora 对齐实现
// ===========================================================================

/**
 * 取当前编辑器的全文与光标位置。
 */
QString MainWindow::currentText() const
{
    QTextEdit *editor = currentEditor();
    return editor ? editor->toPlainText() : QString();
}

int MainWindow::currentCursorPos() const
{
    QTextEdit *editor = currentEditor();
    return editor ? editor->textCursor().position() : 0;
}

/**
 * 应用块级编辑结果到编辑器。
 */
void MainWindow::applyBlockResult(const BlockEditor::Result &r)
{
    if (!r.changed) {
        return;
    }
    QTextEdit *editor = currentEditor();
    if (!editor) {
        return;
    }
    QTextCursor cursor = editor->textCursor();
    const int anchor = cursor.anchor();
    const int pos = cursor.position();

    editor->setPlainText(r.text);
    // 恢复选区
    QTextCursor nc(editor->document());
    const int maxPos = editor->document()->characterCount() - 1;
    nc.setPosition(qBound(0, r.selStart, maxPos));
    if (r.selEnd > r.selStart) {
        nc.setPosition(qBound(0, r.selEnd, maxPos), QTextCursor::KeepAnchor);
    }
    editor->setTextCursor(nc);
    (void)anchor;
    (void)pos;
}

/**
 * 通用行内格式切换。
 *
 * 有选区：包裹 / 取消包裹；无选区：插入带占位符的模板。
 */
void MainWindow::toggleInlineFormat(const QString &id)
{
    QTextEdit *editor = currentEditor();
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

/**
 * 插入指定级别的标题。
 */
void MainWindow::insertHeading(int level)
{
    QTextEdit *editor = currentEditor();
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

/**
 * 切换当前行的块类型（段落 / 引用 / 列表 / 任务列表 / 代码块 / 公式块）。
 */
void MainWindow::toggleBlockType(const QString &type)
{
    const BlockEditor::Result r =
        BlockEditor::toggleBlock(currentText(), currentCursorPos(), type);
    applyBlockResult(r);
}

/**
 * 插入脚注（正文引用 + 文末定义）。
 */
void MainWindow::insertFootnote()
{
    applyBlockResult(BlockEditor::insertBlock(currentText(), currentCursorPos(), "footnote"));
}

/**
 * 插入内容目录 [TOC]。
 */
void MainWindow::insertToc()
{
    applyBlockResult(BlockEditor::insertBlock(currentText(), currentCursorPos(), "toc"));
}

/**
 * 插入 YAML Front Matter。
 */
void MainWindow::insertYamlFrontMatter()
{
    applyBlockResult(BlockEditor::insertBlock(currentText(), currentCursorPos(), "yaml"));
}

/**
 * 插入行内公式。
 */
void MainWindow::insertInlineMath()
{
    toggleInlineFormat("math");
}

/**
 * 在当前行上方插入空段落。
 */
void MainWindow::insertParagraphAbove()
{
    applyBlockResult(BlockEditor::insertParagraph(currentText(), currentCursorPos(), "above"));
}

/**
 * 在当前行下方插入空段落。
 */
void MainWindow::insertParagraphBelow()
{
    applyBlockResult(BlockEditor::insertParagraph(currentText(), currentCursorPos(), "below"));
}

/**
 * 切换任务状态（[ ] <-> [x]）。
 */
void MainWindow::toggleTaskStatus()
{
    QTextEdit *editor = currentEditor();
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

/**
 * 增加列表缩进。
 */
void MainWindow::indentList()
{
    QTextEdit *editor = currentEditor();
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

/**
 * 减少列表缩进。
 */
void MainWindow::outdentList()
{
    QTextEdit *editor = currentEditor();
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

/**
 * 在表格中插入行。
 */
void MainWindow::insertTableRow(bool above)
{
    applyBlockResult(BlockEditor::insertTableRow(currentText(), currentCursorPos(),
                                                 above ? "above" : "below"));
}

/**
 * 删除表格当前行。
 */
void MainWindow::deleteTableRow()
{
    applyBlockResult(BlockEditor::deleteTableRow(currentText(), currentCursorPos()));
}

/**
 * 在表格中插入列。
 */
void MainWindow::insertTableColumn(bool before)
{
    applyBlockResult(BlockEditor::insertTableColumn(currentText(), currentCursorPos(),
                                                    before ? "before" : "after"));
}

/**
 * 删除表格当前列。
 */
void MainWindow::deleteTableColumn()
{
    applyBlockResult(BlockEditor::deleteTableColumn(currentText(), currentCursorPos()));
}

/**
 * 清除所选文本的行内格式。
 */
void MainWindow::clearFormat()
{
    QTextEdit *editor = currentEditor();
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

/**
 * 复制为 Markdown 源码。
 */
void MainWindow::copyAsMarkdown()
{
    QTextEdit *editor = currentEditor();
    if (!editor) {
        return;
    }
    QClipboard *clip = QApplication::clipboard();
    clip->setText(editor->toPlainText());
    statusBar()->showMessage(tr("已复制 Markdown 源码"), 1500);
}

/**
 * 复制为 HTML 代码。
 */
void MainWindow::copyAsHtml()
{
    QTextEdit *editor = currentEditor();
    if (!editor) {
        return;
    }
    QClipboard *clip = QApplication::clipboard();
    clip->setText(generateHtml(editor->toPlainText()));
    statusBar()->showMessage(tr("已复制 HTML 代码"), 1500);
}

/**
 * 以纯文本粘贴（去除剪贴板中的 HTML 格式）。
 */
void MainWindow::pasteAsPlainText()
{
    QClipboard *clip = QApplication::clipboard();
    QTextEdit *editor = currentEditor();
    if (!editor) {
        return;
    }
    editor->insertPlainText(clip->text());
}

/**
 * 以 Markdown 粘贴。
 */
void MainWindow::pasteAsMarkdown()
{
    QClipboard *clip = QApplication::clipboard();
    QTextEdit *editor = currentEditor();
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

/**
 * 删除选区（无选区时删除后一个字符）。
 */
void MainWindow::deleteSelection()
{
    QTextEdit *editor = currentEditor();
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

/**
 * 偏好设置对话框。
 */
void MainWindow::showPreferences()
{
    QDialog dlg(this);
    dlg.setWindowTitle(tr("偏好设置"));
    dlg.setMinimumWidth(420);

    QFormLayout *form = new QFormLayout();
    auto *fontSpin = new QSpinBox(&dlg);
    fontSpin->setRange(8, 48);
    fontSpin->setValue(m_fontSize);
    connect(fontSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int v) {
        updateFontSize(v);
    });
    form->addRow(tr("字号"), fontSpin);

    auto *themeCombo = new QComboBox(&dlg);
    for (const QString &tid : Theme::availableThemes()) {
        themeCombo->addItem(Theme::palette(tid).displayName, tid);
    }
    const int idx = themeCombo->findData(m_themeId);
    if (idx >= 0) {
        themeCombo->setCurrentIndex(idx);
    }
    connect(themeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int i) {
        applyThemeById(themeCombo->itemData(i).toString());
    });
    form->addRow(tr("主题"), themeCombo);

    auto *darkBox = new QCheckBox(tr("深色模式"), &dlg);
    darkBox->setChecked(m_darkMode);
    connect(darkBox, &QCheckBox::toggled, this, &MainWindow::setDarkMode);
    form->addRow(QString(), darkBox);

    auto *smartBox = new QCheckBox(tr("智能标点"), &dlg);
    smartBox->setChecked(m_smartPunctuation->isEnabled());
    connect(smartBox, &QCheckBox::toggled, this, &MainWindow::onSmartPunctuationToggled);
    form->addRow(QString(), smartBox);

    auto *typeBox = new QCheckBox(tr("打字机模式"), &dlg);
    typeBox->setChecked(m_typewriterMode);
    connect(typeBox, &QCheckBox::toggled, this, &MainWindow::setTypewriterMode);
    form->addRow(QString(), typeBox);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    form->addRow(buttons);

    dlg.setLayout(form);
    dlg.exec();
}

/**
 * 打印对话框。
 */
void MainWindow::showPrintDialog()
{
    QTextEdit *editor = currentEditor();
    if (!editor) {
        return;
    }
    QPrinter printer(QPrinter::HighResolution);
    QPrintDialog dlg(&printer, this);
    if (dlg.exec() == QDialog::Accepted) {
        editor->document()->print(&printer);
    }
}

/**
 * 窗口置顶。
 */
void MainWindow::toggleAlwaysOnTop(bool enabled)
{
    if (enabled) {
        setWindowFlag(Qt::WindowStaysOnTopHint, true);
    } else {
        setWindowFlag(Qt::WindowStaysOnTopHint, false);
    }
    show();
}

/**
 * 开发者工具（Qt 原生调试窗口）。
 */
void MainWindow::showDevTools()
{
    // 列出可用的 Qt 调试视图，作为「开发者工具」的轻量替代
    QMenu menu(tr("开发者工具"), this);
    QAction *logAct = menu.addAction(tr("查看输出日志"));
    QAction *objAct = menu.addAction(tr("对象树"));
    QAction *propAct = menu.addAction(tr("属性"));
    menu.exec(QCursor::pos());

    if (logAct->isSelected()) {
        QMessageBox::information(this, tr("开发者工具"),
                                 tr("日志输出：\n%1").arg(qInstallMessageHandler(nullptr)
                                     ? QStringLiteral("(已安装)") : QStringLiteral("(无)")));
    } else if (objAct->isSelected()) {
        QStringList names;
        for (QObject *child : findChildren<QObject*>()) {
            names << QStringLiteral("%1 (%2)")
                          .arg(child->metaObject()->className())
                          .arg(child->objectName());
        }
        QMessageBox::information(this, tr("对象树"),
                                 names.join('\n'));
    } else if (propAct->isSelected()) {
        QMessageBox::information(this, tr("属性"),
                                 tr("窗口尺寸：%1 x %2\n主题：%3\n字体：%4")
                                     .arg(width()).arg(height())
                                     .arg(m_themeId).arg(m_fontSize));
    }
}

/**
 * 打字机模式：光标始终垂直居中。
 */
void MainWindow::setTypewriterMode(bool enabled)
{
    m_typewriterMode = enabled;
    m_typewriterModeAction->setChecked(enabled);

    if (enabled) {
        centerCursorVertically();
    }
    statusBar()->showMessage(enabled ? tr("打字机模式已开启") : tr("打字机模式已关闭"), 2000);
}

/**
 * 把光标所在行垂直居中显示。
 */
void MainWindow::centerCursorVertically()
{
    QTextEdit *editor = currentEditor();
    if (!editor) {
        return;
    }
    const QTextCursor cursor = editor->textCursor();
    const QRect rect = editor->cursorRect(cursor);
    const int target = editor->viewport()->height() / 2;
    const int delta = rect.center().y() - target;
    if (delta != 0) {
        QScrollBar *sb = editor->verticalScrollBar();
        sb->setValue(sb->value() + delta);
    }
}

/**
 * 切换侧边栏页签。
 * 0=文件树 1=大纲 2=文档树
 */
void MainWindow::setSidebarTab(int index)
{
    if (!m_sidebarTabs) {
        return;
    }
    const int count = m_sidebarTabs->count();
    if (index < 0 || index >= count) {
        return;
    }
    m_sidebarTabs->setCurrentIndex(index);
    m_sidebarTabIndex = index;
    if (m_fileTreeDock && m_sidebarVisible) {
        m_fileTreeDock->setVisible(true);
    }

    // 同步勾选状态
    m_outlineAction->setChecked(index == 1);
    m_docTreeAction->setChecked(index == 2);
    m_fileTreeAction->setChecked(index == 0);
}
'''

with io.open(SRC, 'a', encoding='utf-8', newline='\n') as f:
    f.write(IMPL)

print('  Typora 对齐实现已追加')
