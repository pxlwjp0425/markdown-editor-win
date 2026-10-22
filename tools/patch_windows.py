#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Windows 平台适配补丁。

处理的问题：
  1. 编码确定性：Windows 中文系统 locale 可能是 GBK。虽然 Qt6 的 QTextStream
     默认已是 UTF-8，但显式 setEncoding 让行为在任何区域设置下都可预期。
  2. 换行符：Windows 文本文件期望 CRLF。原代码用 QIODevice::Text，
     实际上 QFile 打开时 Text 模式会让 Qt 做换行转换（读时统一为 \n，
     写时转成 \r\n），这一点保持不变即可。
  3. 字体：Linux 上的默认等宽字体在 Windows 不存在，需要回退策略。
  4. 路径分隔符：insertImage 用的是 QFileInfo::fileName()，
     拼进 Markdown 时应使用正斜杠（URL 风格），Windows 反斜杠会破坏渲染。
  5. 高亮器：QRegularExpression 的 ^ $ 默认不按行匹配，
     需加 MultilineOption，否则标题/列表/引用在编辑器里不高亮。
"""
import io
import re
import sys

MAIN = sys.argv[1]
HL = sys.argv[2]


def read(p):
    with io.open(p, 'r', encoding='utf-8') as f:
        return f.read()


def write(p, s):
    with io.open(p, 'w', encoding='utf-8', newline='\n') as f:
        f.write(s)


# ============ 1. mainwindow.cpp ============
src = read(MAIN)
changes = []

# 1.1 打开文件：显式 UTF-8
old = '''    QTextStream in(&file);
    QString content = in.readAll();
    file.close();'''
new = '''    QTextStream in(&file);
    in.setEncoding(QStringConverter::Utf8);
    QString content = in.readAll();
    file.close();'''
if old in src:
    src = src.replace(old, new, 1)
    changes.append('openFile: UTF-8 read')

# 1.2 保存文件：显式 UTF-8
old = '''    QTextStream out(&file);
    out << editor->toPlainText();
    file.close();'''
new = '''    QTextStream out(&file);
    out.setEncoding(QStringConverter::Utf8);
    out << editor->toPlainText();
    out.flush();
    file.close();'''
if old in src:
    src = src.replace(old, new, 1)
    changes.append('saveCurrentTab: UTF-8 write')

# 1.3 导出 HTML：显式 UTF-8
old = '''        QTextStream out(&file);
        out << html;
        file.close();'''
new = '''        QTextStream out(&file);
        out.setEncoding(QStringConverter::Utf8);
        out << html;
        out.flush();
        file.close();'''
if old in src:
    src = src.replace(old, new, 1)
    changes.append('exportToHtml: UTF-8 write')

# 1.4 include QStringConverter
if '#include <QStringConverter>' not in src:
    src = src.replace('#include <QTextStream>',
                      '#include <QTextStream>\n#include <QStringConverter>', 1)
    if '#include <QStringConverter>' not in src:
        src = src.replace('#include <QRegularExpression>',
                          '#include <QRegularExpression>\n#include <QStringConverter>', 1)
    changes.append('include QStringConverter')

# 1.5 编辑器等宽字体：Windows 回退链
old = '''    editor->setFont(QFont("Consolas", m_fontSize));'''
new = '''    // Windows 上 Consolas 存在；Linux 上回退到等宽字体
    QFont monoFont = QFont("Consolas", m_fontSize);
    monoFont.setStyleHint(QFont::Monospace);
    monoFont.setFixedPitch(true);
    editor->setFont(monoFont);'''
if old in src:
    src = src.replace(old, new, 1)
    changes.append('editor font: monospace fallback chain')

# 1.6 Markdown 图片路径：Windows 反斜杠 -> 正斜杠
old = '''        QFileInfo fileInfo(fileName);
        QString markdown = QString("![%1](%2)").arg(fileInfo.fileName()).arg(fileName);
        insertText(markdown);'''
new = '''        QFileInfo fileInfo(fileName);
        // Markdown 链接使用 URL 风格路径，Windows 的反斜杠会破坏渲染
        QString urlPath = QDir::toNativeSeparators(fileName);
        urlPath.replace('\\\\', '/');
        QString markdown = QString("![%1](%2)").arg(fileInfo.fileName()).arg(urlPath);
        insertText(markdown);'''
if old in src:
    src = src.replace(old, new, 1)
    changes.append('insertImage: backslash -> forward slash')

# 1.7 拖放事件处理（Windows 上用鼠标拖文件到编辑器很常见）
#     原代码 installEventFilter(this) 但没有实现 dragEnterEvent/dropEvent
old = '''void MainWindow::keyPressEvent(QKeyEvent *event)'''
new = '''void MainWindow::dragEnterEvent(QDragEnterEvent *event)
{
    // 允许从资源管理器拖入 .md 文件
    if (event->mimeData()->hasUrls()) {
        event->acceptProposedAction();
    } else {
        QMainWindow::dragEnterEvent(event);
    }
}

void MainWindow::dropEvent(QDropEvent *event)
{
    const QList<QUrl> urls = event->mimeData()->urls();
    for (const QUrl &url : urls) {
        if (url.isLocalFile()) {
            QString path = url.toLocalFile();
            QFileInfo info(path);
            if (info.isFile() &&
                (info.suffix() == "md" || info.suffix() == "markdown" || info.suffix() == "txt")) {
                openFile(path);
                event->acceptProposedAction();
                return;
            }
        }
    }
    QMainWindow::dropEvent(event);
}

void MainWindow::keyPressEvent(QKeyEvent *event)'''
if old in src:
    src = src.replace(old, new, 1)
    changes.append('drag & drop: open .md file')

write(MAIN, src)
print('mainwindow.cpp:')
for c in changes:
    print('  -', c)

# ============ 2. markdownhighlighter.cpp ============
hl = read(HL)
hl_changes = []

# QRegularExpression 的 ^ $ 默认不跨行，需 MultilineOption
old = '''    // 标题规则
    QRegularExpression heading1("^# .+$");
    QRegularExpression heading2("^## .+$");
    QRegularExpression heading3("^### .+$");
    QRegularExpression heading4("^#### .+$");
    QRegularExpression heading5("^##### .+$");
    QRegularExpression heading6("^###### .+$");
    
    // 代码块规则
    QRegularExpression codeBlockStart("^```");
    QRegularExpression codeBlockEnd("^```$");
    
    // 行内代码
    QRegularExpression inlineCode("`[^`]+`");
    
    // 链接
    QRegularExpression link("\\\\[([^\\\\]]+)\\\\]\\\\(([^)]+)\\\\)");
    QRegularExpression image("!\\\\[([^\\\\]]*)\\\\]\\\\(([^)]+)\\\\)");
    
    // 粗体和斜体
    QRegularExpression bold("\\\\*\\\\*[^*]+\\\\*\\\\*");
    QRegularExpression boldAlt("__[^_]+__");
    QRegularExpression italic("\\\\*[^*]+\\\\*");
    QRegularExpression italicAlt("_[^_]+_");
    QRegularExpression strikethrough("~~[^~]+~~");
    
    // 列表标记
    QRegularExpression bulletList("^\\\\s*[-*+]\\\\s");
    QRegularExpression numberedList("^\\\\s*\\\\d+\\\\.\\\\s");
    
    // 引用
    QRegularExpression blockquote("^>\\\\s");'''

new = '''    // 行锚定规则需要 MultilineOption：
    // QRegExp 的 ^ $ 默认按行匹配，QRegularExpression 默认按整串匹配，
    // 不加此选项则标题/列表/引用在编辑器中完全不高亮。
    const auto ml = QRegularExpression::MultilineOption;

    // 标题规则
    QRegularExpression heading1("^# .+$", ml);
    QRegularExpression heading2("^## .+$", ml);
    QRegularExpression heading3("^### .+$", ml);
    QRegularExpression heading4("^#### .+$", ml);
    QRegularExpression heading5("^##### .+$", ml);
    QRegularExpression heading6("^###### .+$", ml);
    
    // 代码块规则
    QRegularExpression codeBlockStart("^```", ml);
    QRegularExpression codeBlockEnd("^```$", ml);
    
    // 行内代码
    QRegularExpression inlineCode("`[^`]+`");
    
    // 链接
    QRegularExpression link("\\\\[([^\\\\]]+)\\\\]\\\\(([^)]+)\\\\)");
    QRegularExpression image("!\\\\[([^\\\\]]*)\\\\]\\\\(([^)]+)\\\\)");
    
    // 粗体和斜体
    QRegularExpression bold("\\\\*\\\\*[^*]+\\\\*\\\\*");
    QRegularExpression boldAlt("__[^_]+__");
    QRegularExpression italic("\\\\*[^*]+\\\\*");
    QRegularExpression italicAlt("_[^_]+_");
    QRegularExpression strikethrough("~~[^~]+~~");
    
    // 列表标记
    QRegularExpression bulletList("^\\\\s*[-*+]\\\\s", ml);
    QRegularExpression numberedList("^\\\\s*\\\\d+\\\\.\\\\s", ml);
    
    // 引用
    QRegularExpression blockquote("^>\\\\s", ml);'''

if old in hl:
    hl = hl.replace(old, new, 1)
    hl_changes.append('anchored regex: MultilineOption')
else:
    print('  WARNING: highlighter pattern block not matched')

write(HL, hl)
print('markdownhighlighter.cpp:')
for c in hl_changes:
    print('  -', c)
