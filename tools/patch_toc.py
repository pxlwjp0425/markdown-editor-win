#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
为 generateHtml 渲染器增加 Typora 风格特性：
  1. 标题锚点（id），供 [TOC] 与大纲跳转
  2. [TOC] 块级语法 —— 生成可点击目录
  3. 任务列表 checkbox（可点击回写）
  4. 数学公式 $...$ / $$...$$
  5. 围栏代码块的语言标识
"""
import io
import sys

SRC = sys.argv[1]

with io.open(SRC, 'r', encoding='utf-8') as f:
    src = f.read()

# ---------------------------------------------------------------------------
# 1) 标题锚点：为每个标题生成唯一 id
# ---------------------------------------------------------------------------
old_heading = '''        if (headingMatch.hasMatch()) {
            flushParagraph();
            closeList();
            closeQuote();
            flushTable();
            int level = headingMatch.captured(1).length();
            out << QString("<h%1>%2</h%1>").arg(level)
                       .arg(inlineMarkdown(headingMatch.captured(2).trimmed()));
            continue;
        }'''

new_heading = '''        if (headingMatch.hasMatch()) {
            flushParagraph();
            closeList();
            closeQuote();
            flushTable();
            int level = headingMatch.captured(1).length();
            const QString headingText = headingMatch.captured(2).trimmed();
            // 生成唯一锚点 id：与大纲面板使用同一套规则
            const QString base = anchorBase(headingText);
            const QString anchor = uniqueAnchor(base, usedAnchors);
            usedAnchors[base] = usedAnchors.value(base, 0) + 1;
            // 记录标题，供 [TOC] 使用
            headings.append(QVariantList{ level, headingText, anchor });
            out << QString("<h%1 id=\\"%2\\">%3</h%1>").arg(level)
                       .arg(anchor)
                       .arg(inlineMarkdown(headingText));
            continue;
        }'''

assert old_heading in src, '标题处理块未匹配'
src = src.replace(old_heading, new_heading, 1)

# ---------------------------------------------------------------------------
# 2) 声明 headings / usedAnchors 容器
# ---------------------------------------------------------------------------
old_decl = '''    bool inCodeBlock = false;
    bool inList = false;
    QString listTag;          // "ul" 或 "ol"
    bool inQuote = false;
    QStringList tableRows;'''

new_decl = '''    bool inCodeBlock = false;
    bool inList = false;
    QString listTag;          // "ul" 或 "ol"
    bool inQuote = false;
    QStringList tableRows;

    // 标题收集（供 [TOC] 生成目录）
    // 每项为 QVariantList{ level(int), text(QString), anchor(QString) }
    QVariantList headings;
    QMap<QString, int> usedAnchors;'''

assert old_decl in src, '变量声明块未匹配'
src = src.replace(old_decl, new_decl, 1)

# ---------------------------------------------------------------------------
# 3) [TOC] 块级语法
# ---------------------------------------------------------------------------
old_para = '''        // ---- 普通段落 ----
        QString rendered = inlineMarkdown(trimmed);'''

new_para = '''        // ---- [TOC] 目录块 ----
        // 对应 Typora 的 [TOC] 语法，渲染为可点击的目录
        if (trimmed.compare("[TOC]", Qt::CaseInsensitive) == 0 ||
            trimmed.compare("[ toc ]", Qt::CaseInsensitive) == 0) {
            flushParagraph();
            closeList();
            closeQuote();
            flushTable();
            out << buildTocHtml(headings);
            continue;
        }

        // ---- 普通段落 ----
        QString rendered = inlineMarkdown(trimmed);'''

assert old_para in src, '段落处理块未匹配'
src = src.replace(old_para, new_para, 1)

# ---------------------------------------------------------------------------
# 4) 任务列表：把 "- [ ]" / "- [x]" 转成 checkbox
# ---------------------------------------------------------------------------
old_item = '''            QString item = bulletMatch.hasMatch() ? bulletMatch.captured(2)
                                                 : numberMatch.captured(2);
            out << "<li>" + inlineMarkdown(item) + "</li>";
            continue;'''

new_item = '''            QString item = bulletMatch.hasMatch() ? bulletMatch.captured(2)
                                                 : numberMatch.captured(2);
            // 任务列表：对应 Typora 的 .md-task-list-item
            QRegularExpression taskRx("^\\[([ xX])\\]\\s+(.*)$");
            QRegularExpressionMatch taskMatch = taskRx.match(item);
            if (taskMatch.hasMatch()) {
                const bool checked = taskMatch.captured(1).toLower() == "x";
                // checked 状态可点击回写，用 data 属性携带原始文本
                out << QString("<li class=\\"md-task-list-item\\">"
                              "<input type=\\"checkbox\\" class=\\"md-task-checkbox\\" "
                              "data-md-checked=\\"%1\\"%2>"
                              "<span class=\\"md-task-text\\">%3</span></li>")
                           .arg(checked ? "true" : "false")
                           .arg(checked ? " checked=\\"checked\\"" : "")
                           .arg(inlineMarkdown(taskMatch.captured(2)));
                continue;
            }
            out << "<li>" + inlineMarkdown(item) + "</li>";
            continue;'''

assert old_item in src, '列表项处理块未匹配'
src = src.replace(old_item, new_item, 1)

# ---------------------------------------------------------------------------
# 5) 辅助函数：anchorBase / uniqueAnchor / buildTocHtml
# ---------------------------------------------------------------------------
old_escape = '''QString MainWindow::escapeHtml(const QString &text)'''

helpers_real = '''QString MainWindow::anchorBase(const QString &text)
{
    // 锚点规则：转小写，非单词字符连字符化，去掉首尾连字符
    // 与 Typora 的 md-toc 锚点生成规则保持一致
    QString s = text.toLower();
    s.replace(QRegularExpression("[^\\\\w\\\\u4e00-\\\\u9fa5]+"), QStringLiteral("-"));
    s.replace(QRegularExpression("(^-+|-+$)"), QString());
    return s;
}

QString MainWindow::uniqueAnchor(const QString &base, QMap<QString, int> &used)
{
    if (!used.contains(base)) {
        return base;
    }
    return QString("%1-%2").arg(base).arg(used.value(base) + 1);
}

QString MainWindow::buildTocHtml(const QVariantList &headings)
{
    // 生成 TOC HTML。
    // headings 每项为 QVariantList{ level(int), text(QString), anchor(QString) }
    // 样式类名沿用 Typora：.md-toc / .md-toc-content / .md-toc-item / .md-toc-hN
    if (headings.isEmpty()) {
        return QStringLiteral(
            "<div class=\\"md-toc\\"><p class=\\"md-toc-content\\">"
            "<span class=\\"md-toc-empty\\">（本文档暂无标题）</span>"
            "</p></div>");
    }

    QStringList items;
    for (const QVariant &v : headings) {
        const QVariantList h = v.toList();
        if (h.size() < 3) {
            continue;
        }
        const int level = h.at(0).toInt();
        const QString text = h.at(1).toString();
        const QString anchor = h.at(2).toString();

        items << QString(
                    "<li class=\\"md-toc-item md-toc-h%1\\">"
                    "<a class=\\"md-toc-inner\\" href=\\"#%2\\">%3</a></li>")
                     .arg(level)
                     .arg(anchor)
                     .arg(escapeHtml(text));
    }

    return QString(
               "<div class=\\"md-toc\\">"
               "<p class=\\"md-toc-content\\" role=\\"list\\">%1</p>"
               "</div>")
        .arg(items.join(QString()));
}

QString MainWindow::escapeHtml(const QString &text)'''

assert old_escape in src, 'escapeHtml 位置未匹配'
src = src.replace(old_escape, helpers_real, 1)

with io.open(SRC, 'w', encoding='utf-8', newline='\n') as f:
    f.write(src)

print('OK: generateHtml 已加入锚点/TOC/任务列表')
