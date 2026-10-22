#include "htmlrenderer.h"

#include <QMap>

// ===========================================================================
// 基础工具
// ===========================================================================

QString HtmlRenderer::escapeHtml(const QString &text)
{
    QString s = text;
    s.replace(QLatin1Char('&'), QLatin1String("&amp;"));
    s.replace(QLatin1Char('<'), QLatin1String("&lt;"));
    s.replace(QLatin1Char('>'), QLatin1String("&gt;"));
    return s;
}

QString HtmlRenderer::escapeAttr(const QString &text)
{
    QString s = text;
    s.replace(QLatin1Char('&'), QLatin1String("&amp;"));
    s.replace(QLatin1Char('<'), QLatin1String("&lt;"));
    s.replace(QLatin1Char('>'), QLatin1String("&gt;"));
    s.replace(QLatin1Char('"'), QLatin1String("&quot;"));
    return s;
}

// ===========================================================================
// 行内渲染
// ===========================================================================

/**
 * 渲染行内片段。
 *
 * 嵌套关系由 IR 里的布尔标志表达（粗体+斜体可以同时为真），
 * 因此不存在「标签套标签」的问题，顺序固定即可。
 *
 * 链接在最外层：IR 解析链接时会递归处理链接文字，
 * 把 link 作为基础标志传下去，所以链接文字里的粗体等都在 <a> 内部。
 */
QString HtmlRenderer::renderInlines(const QList<DocInline> &inlines)
{
    QString s;
    for (const DocInline &i : inlines) {
        // 图片
        if (!i.imageSrc.isEmpty()) {
            s += QStringLiteral("<img src=\"%1\" alt=\"%2\">")
                     .arg(escapeAttr(i.imageSrc), escapeAttr(i.imageAlt));
            continue;
        }

        QString t = escapeHtml(i.text);

        if (i.math) {
            t = QStringLiteral("<code class=\"math\">%1</code>").arg(t);
        } else if (i.code) {
            t = QStringLiteral("<code>%1</code>").arg(t);
        } else {
            if (i.highlight) {
                t = QStringLiteral("<mark>%1</mark>").arg(t);
            }
            if (i.strike) {
                t = QStringLiteral("<del>%1</del>").arg(t);
            }
            if (i.bold && i.italic) {
                t = QStringLiteral("<strong><em>%1</em></strong>").arg(t);
            } else if (i.bold) {
                t = QStringLiteral("<strong>%1</strong>").arg(t);
            } else if (i.italic) {
                t = QStringLiteral("<em>%1</em>").arg(t);
            }
            if (i.superscript) {
                t = QStringLiteral("<sup>%1</sup>").arg(t);
            }
            if (i.subscript) {
                t = QStringLiteral("<sub>%1</sub>").arg(t);
            }
        }

        if (!i.link.isEmpty()) {
            t = QStringLiteral("<a href=\"%1\">%2</a>").arg(escapeAttr(i.link), t);
        }

        s += t;
    }
    return s;
}

// ===========================================================================
// 标题与目录
// ===========================================================================

QList<HtmlRenderer::Heading> HtmlRenderer::collectHeadings(const QList<DocBlock> &blocks)
{
    QList<Heading> result;
    QMap<QString, int> used;

    for (const DocBlock &b : blocks) {
        if (b.type != DocBlock::Heading) {
            continue;
        }
        Heading h;
        h.level = b.level;
        // 锚点基于标题原文计算（不是解析后的行内片段），
        // 这样大纲面板、目录、标题 id 三者用的是同一个字符串。
        h.text = b.raw;
        h.anchor = DocModel::uniqueAnchor(DocModel::anchorBase(b.raw), used);
        h.sourceLine = b.sourceLine;
        result.append(h);
    }
    return result;
}

/**
 * 生成 [TOC] 的 HTML。
 *
 * 样式类名沿用 Typora：.md-toc / .md-toc-content / .md-toc-item / .md-toc-hN
 */
QString HtmlRenderer::renderToc(const QList<Heading> &headings)
{
    if (headings.isEmpty()) {
        return QStringLiteral(
            "<div class=\"md-toc\"><p class=\"md-toc-content\">"
            "<span class=\"md-toc-empty\">（本文档暂无标题）</span>"
            "</p></div>");
    }

    QStringList items;
    for (const Heading &h : headings) {
        items << QStringLiteral(
                    "<li class=\"md-toc-item md-toc-h%1\">"
                    "<a class=\"md-toc-inner\" href=\"#%2\">%3</a></li>")
                     .arg(h.level)
                     .arg(escapeAttr(h.anchor))
                     .arg(escapeHtml(h.text));
    }

    return QStringLiteral(
               "<div class=\"md-toc\">"
               "<p class=\"md-toc-content\" role=\"list\">%1</p>"
               "</div>")
        .arg(items.join(QLatin1Char('\n')));
}

// ===========================================================================
// 块级渲染
// ===========================================================================

/**
 * 渲染 HTML body 片段。
 */
QString HtmlRenderer::renderBody(const QList<DocBlock> &blocks)
{
    // 先收集标题：[TOC] 可能出现在文档靠前位置，
    // 那时后面的标题还没被扫描到，必须先扫一遍全文。
    const QList<Heading> headings = collectHeadings(blocks);

    QString body;

    // 当前打开的列表层级。
    // 每层记录标签名与「该层的 <li> 是否还开着」——
    // 嵌套列表必须生成在父项的 <li> 内部（HTML 语义要求），
    // 所以父项的 </li> 要等到子列表结束后才补。
    struct ListLevel {
        QString tag;          // "ul" 或 "ol"
        bool itemOpen = false;// 该层是否有一个未闭合的 <li>
    };
    QList<ListLevel> listStack;

    // 第 N 个标题块对应 headings[N]
    int headingIndex = 0;

    auto closeAllLists = [&]() {
        while (!listStack.isEmpty()) {
            if (listStack.last().itemOpen) {
                body += QStringLiteral("</li>");
                listStack.last().itemOpen = false;
            }
            body += QStringLiteral("</%1>").arg(listStack.last().tag);
            listStack.removeLast();
        }
    };

    for (const DocBlock &b : blocks) {
        switch (b.type) {
        case DocBlock::Heading: {
            closeAllLists();
            QString anchor;
            if (headingIndex < headings.size()) {
                anchor = headings.at(headingIndex).anchor;
            }
            ++headingIndex;
            body += QStringLiteral("<h%1 id=\"%2\">%3</h%1>")
                        .arg(b.level)
                        .arg(escapeAttr(anchor))
                        .arg(renderInlines(b.inlines));
            break;
        }
        case DocBlock::Paragraph:
            closeAllLists();
            body += QStringLiteral("<p>%1</p>").arg(renderInlines(b.inlines));
            break;
        case DocBlock::ListItem: {
            const QString tag = b.ordered ? QStringLiteral("ol") : QStringLiteral("ul");
            // 层级：level 0 是最外层，嵌套一层就多开一个列表层级
            const int wantDepth = b.level + 1;

            // 1. 离开更深的层级
            while (listStack.size() > wantDepth) {
                if (listStack.last().itemOpen) {
                    body += QStringLiteral("</li>");
                    listStack.last().itemOpen = false;
                }
                body += QStringLiteral("</%1>").arg(listStack.last().tag);
                listStack.removeLast();
            }

            if (listStack.size() < wantDepth) {
                // 2a. 进入更深一层：新列表开在父项未闭合的 <li> 内部
                ListLevel lv;
                lv.tag = tag;
                listStack.append(lv);
                body += QStringLiteral("<%1>").arg(tag);
            } else {
                // 2b. 同层：先闭合上一个列表项
                if (listStack.last().itemOpen) {
                    body += QStringLiteral("</li>");
                    listStack.last().itemOpen = false;
                }
                // 同层但列表类型变了（ul 变 ol）：关掉重开
                if (listStack.last().tag != tag) {
                    body += QStringLiteral("</%1>").arg(listStack.last().tag);
                    listStack.removeLast();
                    ListLevel lv;
                    lv.tag = tag;
                    listStack.append(lv);
                    body += QStringLiteral("<%1>").arg(tag);
                }
            }

            if (b.isTask) {
                // 任务列表：QTextDocument 不支持 <input type=checkbox>，
                // 但 anchorClicked 信号可靠可捕获，因此用 ☐/☑ 字形 + #md-task:N 锚点，
                // N 为该任务在源码中的行号（点击后回写源码的 [ ] / [x]）。
                body += QStringLiteral("<li class=\"md-task-list-item\">"
                                       "<a class=\"md-task-box\" href=\"#md-task:%1\">%2</a>"
                                       "<span class=\"md-task-text\">%3</span>")
                            .arg(b.sourceLine)
                            .arg(b.checked ? QStringLiteral("☑") : QStringLiteral("☐"))
                            .arg(renderInlines(b.inlines));
            } else {
                body += QStringLiteral("<li>%1").arg(renderInlines(b.inlines));
            }
            listStack.last().itemOpen = true;
            break;
        }
        case DocBlock::Quote: {
            closeAllLists();
            // 嵌套引用：level 是层数，一层一个 <blockquote>
            const int depth = qMax(1, b.level);
            for (int i = 0; i < depth; ++i) {
                body += QStringLiteral("<blockquote>");
            }
            body += QStringLiteral("<p>%1</p>").arg(renderInlines(b.inlines));
            for (int i = 0; i < depth; ++i) {
                body += QStringLiteral("</blockquote>");
            }
            break;
        }
        case DocBlock::CodeBlock:
            closeAllLists();
            body += QStringLiteral("<pre><code%1>%2</code></pre>")
                        .arg(b.lang.isEmpty()
                                 ? QString()
                                 : QStringLiteral(" class=\"language-%1\"").arg(escapeAttr(b.lang)))
                        .arg(escapeHtml(b.codeLines.join(QLatin1Char('\n'))));
            break;
        case DocBlock::MathBlock:
            closeAllLists();
            body += QStringLiteral("<pre class=\"math\">%1</pre>")
                        .arg(escapeHtml(b.codeLines.join(QLatin1Char('\n'))));
            break;
        case DocBlock::Table: {
            closeAllLists();
            if (b.tableRows.isEmpty()) {
                break;
            }
            body += QStringLiteral("<table><thead><tr>");
            for (int c = 0; c < b.tableRows.at(0).size(); ++c) {
                const QString align = (c < b.tableAligns.size()) ? b.tableAligns.at(c) : QString();
                body += QStringLiteral("<th%1>%2</th>")
                            .arg(align.isEmpty() ? QString()
                                                 : QStringLiteral(" align=\"%1\"").arg(align))
                            .arg(renderInlines(
                                DocModel::parseInlines(b.tableRows.at(0).at(c))));
            }
            body += QStringLiteral("</tr></thead><tbody>");
            for (int r = 1; r < b.tableRows.size(); ++r) {
                body += QStringLiteral("<tr>");
                for (int c = 0; c < b.tableRows.at(r).size(); ++c) {
                    const QString align = (c < b.tableAligns.size()) ? b.tableAligns.at(c) : QString();
                    body += QStringLiteral("<td%1>%2</td>")
                                .arg(align.isEmpty() ? QString()
                                                     : QStringLiteral(" align=\"%1\"").arg(align))
                                .arg(renderInlines(
                                    DocModel::parseInlines(b.tableRows.at(r).at(c))));
                }
                body += QStringLiteral("</tr>");
            }
            body += QStringLiteral("</tbody></table>");
            break;
        }
        case DocBlock::HorizontalRule:
            closeAllLists();
            body += QStringLiteral("<hr>");
            break;
        case DocBlock::Toc:
            closeAllLists();
            body += renderToc(headings);
            break;
        case DocBlock::RawHtml:
            closeAllLists();
            body += b.raw;
            break;
        }
    }

    closeAllLists();
    return body;
}

