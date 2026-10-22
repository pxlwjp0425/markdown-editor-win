#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
用逐行状态机解析器替换 MainWindow::generateHtml 的原实现。

原实现问题：
  1. 依赖 QRegExp 语义（Qt6 已移除），且 QRegExp 的 ^ $ 默认按行匹配，
     换成 QRegularExpression 后若不加 MultilineOption，标题/列表/引用全部失效。
  2. "while (rx.indexIn(html) != -1) { html.replace(rx.cap(0), ...) }" 存在
     重复匹配/死循环风险（替换后的 <h1>xxx</h1> 仍可能被后续规则再次命中）。
  3. 代码块先被正则替换成 <pre>，后续逐行段落处理无法识别已生成的标签，
     段落包裹逻辑混乱，容易产生嵌套 <p><pre>。

新实现：逐行扫描 + 状态机，正确处理
  围栏代码块 / ATX 标题 / 有序列表 / 无序列表 / 引用块 / 水平线 /
  表格 / 段落，并对行内元素（行内代码、粗体、斜体、删除线、链接、图片）做转义与替换。
"""
import io
import sys

SRC = sys.argv[1]

NEW_FUNC = r'''QString MainWindow::generateHtml(const QString &markdown)
{
    // 逐行状态机解析：先按块级结构切分，再处理行内元素。
    // 这样避免了「正则全局替换 + while 循环」方案的死循环与标签嵌套问题。
    QStringList lines = markdown.split('\n');
    QStringList out;
    out.reserve(lines.size() + 16);

    bool inCodeBlock = false;
    bool inList = false;
    QString listTag;          // "ul" 或 "ol"
    bool inQuote = false;
    QStringList tableRows;

    // 段落缓冲：连续的非空普通行合并为一个 <p>
    QString paragraph;

    auto flushParagraph = [&]() {
        if (!paragraph.isEmpty()) {
            out << "<p>" + paragraph + "</p>";
            paragraph.clear();
        }
    };
    auto closeList = [&]() {
        if (inList) {
            out << "</" + listTag + ">";
            inList = false;
            listTag.clear();
        }
    };
    auto closeQuote = [&]() {
        if (inQuote) {
            out << "</blockquote>";
            inQuote = false;
        }
    };
    auto flushTable = [&]() {
        if (tableRows.isEmpty()) {
            return;
        }
        // 第一行为表头，第二行为分隔线（---），其余为数据行
        QStringList head = tableRows.value(0).split('|');
        QStringList body = tableRows.mid(2);

        out << "<table><thead><tr>";
        for (const QString &cell : head) {
            out << "<th>" + inlineMarkdown(cell.trimmed()) + "</th>";
        }
        out << "</tr></thead><tbody>";
        for (const QString &row : body) {
            QStringList cells = row.split('|');
            out << "<tr>";
            for (const QString &cell : cells) {
                out << "<td>" + inlineMarkdown(cell.trimmed()) + "</td>";
            }
            out << "</tr>";
        }
        out << "</tbody></table>";
        tableRows.clear();
    };

    for (int i = 0; i < lines.size(); ++i) {
        QString line = lines.at(i);
        QString trimmed = line.trimmed();

        // ---- 围栏代码块 ----
        if (trimmed.startsWith("```")) {
            if (!inCodeBlock) {
                flushParagraph();
                closeList();
                closeQuote();
                flushTable();
                inCodeBlock = true;
                // 语言标识：```cpp
                QString lang = trimmed.mid(3).trimmed();
                out << "<pre><code" +
                        (lang.isEmpty() ? QString()
                                        : QString(" class=\"language-%1\"").arg(lang)) +
                        ">";
            } else {
                out << "</code></pre>";
                inCodeBlock = false;
            }
            continue;
        }
        if (inCodeBlock) {
            out << escapeHtml(line);
            continue;
        }

        // ---- 空行：结束所有块级上下文 ----
        if (trimmed.isEmpty()) {
            flushParagraph();
            closeList();
            closeQuote();
            flushTable();
            continue;
        }

        // ---- 水平线 ----
        if (QRegularExpression("^([-*_])\\s*(\\1\\s*){2,}$")
                .match(trimmed).hasMatch()) {
            flushParagraph();
            closeList();
            closeQuote();
            flushTable();
            out << "<hr>";
            continue;
        }

        // ---- ATX 标题 ----
        QRegularExpression headingRx("^(#{1,6})\\s+(.*)$");
        QRegularExpressionMatch headingMatch = headingRx.match(trimmed);
        if (headingMatch.hasMatch()) {
            flushParagraph();
            closeList();
            closeQuote();
            flushTable();
            int level = headingMatch.captured(1).length();
            out << QString("<h%1>%2</h%1>").arg(level)
                       .arg(inlineMarkdown(headingMatch.captured(2).trimmed()));
            continue;
        }

        // ---- 表格：当前行含 | 且下一行是分隔行 ----
        bool isTableRow = trimmed.contains('|');
        bool nextIsSeparator = false;
        if (isTableRow && i + 1 < lines.size()) {
            QString next = lines.at(i + 1).trimmed();
            nextIsSeparator = QRegularExpression("^\\|?\\s*:?-{2,}:?\\s*(\\|\\s*:?-{2,}:?\\s*)*\\|?$")
                                  .match(next).hasMatch();
        }
        if (isTableRow && nextIsSeparator) {
            flushParagraph();
            closeList();
            closeQuote();
            tableRows.clear();
            tableRows << trimmed;
            ++i;  // 跳过分隔行
            continue;
        }
        if (!tableRows.isEmpty()) {
            if (isTableRow) {
                tableRows << trimmed;
                continue;
            }
            flushTable();
        }

        // ---- 引用块 ----
        if (trimmed.startsWith(">")) {
            flushParagraph();
            closeList();
            QString content = trimmed;
            if (content.startsWith("> ")) {
                content = content.mid(2);
            } else if (content.startsWith(">")) {
                content = content.mid(1);
            }
            if (!inQuote) {
                out << "<blockquote>";
                inQuote = true;
            }
            out << inlineMarkdown(content);
            continue;
        }
        closeQuote();

        // ---- 无序列表 ----
        QRegularExpression bulletRx("^([-+*])\\s+(.*)$");
        QRegularExpressionMatch bulletMatch = bulletRx.match(trimmed);
        // ---- 有序列表 ----
        QRegularExpression numberRx("^(\\d+)[.)]\\s+(.*)$");
        QRegularExpressionMatch numberMatch = numberRx.match(trimmed);

        if (bulletMatch.hasMatch() || numberMatch.hasMatch()) {
            flushParagraph();
            QString wantTag = bulletMatch.hasMatch() ? "ul" : "ol";
            if (!inList) {
                out << QString("<%1>").arg(wantTag);
                inList = true;
                listTag = wantTag;
            } else if (listTag != wantTag) {
                // 列表类型切换，闭合后重新开启
                out << "</" + listTag + ">";
                out << QString("<%1>").arg(wantTag);
                listTag = wantTag;
            }
            QString item = bulletMatch.hasMatch() ? bulletMatch.captured(2)
                                                 : numberMatch.captured(2);
            out << "<li>" + inlineMarkdown(item) + "</li>";
            continue;
        }
        closeList();

        // ---- 普通段落 ----
        QString rendered = inlineMarkdown(trimmed);
        if (paragraph.isEmpty()) {
            paragraph = rendered;
        } else {
            paragraph += "\n" + rendered;
        }
    }

    // 收尾
    if (inCodeBlock) {
        out << "</code></pre>";
    }
    flushParagraph();
    closeList();
    closeQuote();
    flushTable();

    QString body = out.join("\n");

    // 构建完整的HTML页面
    QString css = getDefaultCss();

    QString fullHtml = QString(
        "<!DOCTYPE html>\n"
        "<html>\n"
        "<head>\n"
        "<meta charset=\"UTF-8\">\n"
        "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">\n"
        "<style>%1</style>\n"
        "<script>%2</script>\n"
        "</head>\n"
        "<body>\n"
        "<div class=\"markdown-body\">\n"
        "%3\n"
        "</div>\n"
        "</body>\n"
        "</html>"
    ).arg(css).arg(getDefaultJs()).arg(body);

    return fullHtml;
}

QString MainWindow::escapeHtml(const QString &text)
{
    QString escaped = text;
    escaped.replace("&", "&amp;");
    escaped.replace("<", "&lt;");
    escaped.replace(">", "&gt;");
    escaped.replace("\"", "&quot;");
    return escaped;
}

QString MainWindow::inlineMarkdown(const QString &text)
{
    // 行内元素处理顺序：先提取行内代码（内部不做其它替换），再处理其余标记。
    QString result;
    int cursor = 0;
    const QString src = text;

    while (cursor < src.length()) {
        int codeStart = src.indexOf('`', cursor);
        if (codeStart < 0) {
            result += escapeHtml(src.mid(cursor));
            break;
        }
        result += escapeHtml(src.mid(cursor, codeStart - cursor));
        int codeEnd = src.indexOf('`', codeStart + 1);
        if (codeEnd < 0) {
            // 没有闭合的反引号，当作普通字符
            result += escapeHtml(src.mid(codeStart));
            break;
        }
        QString code = src.mid(codeStart + 1, codeEnd - codeStart - 1);
        result += "<code>" + escapeHtml(code) + "</code>";
        cursor = codeEnd + 1;
    }

    // 图片：![alt](url)
    result.replace(QRegularExpression("!\\[([^\\]]*)\\]\\(([^)]+)\\)"), "<img src=\"\\2\" alt=\"\\1\">");
    // 链接：[text](url)
    result.replace(QRegularExpression("\\[([^\\]]+)\\]\\(([^)]+)\\)"), "<a href=\"\\2\">\\1</a>");
    // 粗体
    result.replace(QRegularExpression("\\*\\*(.+?)\\*\\*"), "<strong>\\1</strong>");
    // 删除线
    result.replace(QRegularExpression("~~(.+?)~~"), "<del>\\1</del>");
    // 斜体（放在粗体之后，避免与 ** 冲突）
    result.replace(QRegularExpression("\\*(.+?)\\*"), "<em>\\1</em>");
    result.replace(QRegularExpression("(?<!\\w)_(.+?)_(?!\\w)"), "<em>\\1</em>");

    return result;
}
'''

with io.open(SRC, 'r', encoding='utf-8') as f:
    src = f.read()

start = src.index('QString MainWindow::generateHtml(const QString &markdown)')
end_marker = 'QString MainWindow::getDefaultCss()'
end = src.index(end_marker)

new_src = src[:start] + NEW_FUNC + '\n' + src[end:]

with io.open(SRC, 'w', encoding='utf-8', newline='\n') as f:
    f.write(new_src)

print('OK: generateHtml replaced, new length =', len(new_src))
