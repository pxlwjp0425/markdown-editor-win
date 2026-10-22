#include "exporter.h"

#include <QDateTime>
#include <QRegularExpression>
#include <QTextDocument>
#include <QUuid>
#include <QXmlStreamWriter>

#include "zipwriter.h"

// ===========================================================================
// 格式清单（顺序与 Typora「文件 > 导出」一致）
// ===========================================================================

QList<Exporter::Format> Exporter::formats()
{
    return {
        {QStringLiteral("html"),     QStringLiteral("HTML"),
         QStringLiteral("HTML 文件 (*.html *.htm)"), QStringLiteral("html")},
        {QStringLiteral("pdf"),      QStringLiteral("PDF"),
         QStringLiteral("PDF 文件 (*.pdf)"), QStringLiteral("pdf")},
        {QStringLiteral("docx"),     QStringLiteral("Word (.docx)"),
         QStringLiteral("Word 文档 (*.docx)"), QStringLiteral("docx")},
        {QStringLiteral("odt"),      QStringLiteral("OpenOffice (.odt)"),
         QStringLiteral("OpenDocument 文档 (*.odt)"), QStringLiteral("odt")},
        {QStringLiteral("epub"),     QStringLiteral("EPUB"),
         QStringLiteral("EPUB 电子书 (*.epub)"), QStringLiteral("epub")},
        {QStringLiteral("latex"),    QStringLiteral("LaTeX"),
         QStringLiteral("LaTeX 源文件 (*.tex)"), QStringLiteral("tex")},
        {QStringLiteral("rtf"),      QStringLiteral("RTF"),
         QStringLiteral("RTF 文档 (*.rtf)"), QStringLiteral("rtf")},
        {QStringLiteral("opml"),     QStringLiteral("OPML"),
         QStringLiteral("OPML 大纲 (*.opml)"), QStringLiteral("opml")},
        {QStringLiteral("mediawiki"),QStringLiteral("Wiki (MediaWiki)"),
         QStringLiteral("MediaWiki 文本 (*.wiki *.mediawiki)"), QStringLiteral("wiki")},
        {QStringLiteral("rst"),      QStringLiteral("reStructuredText"),
         QStringLiteral("reStructuredText (*.rst)"), QStringLiteral("rst")},
        {QStringLiteral("textile"),  QStringLiteral("Textile"),
         QStringLiteral("Textile 文本 (*.textile)"), QStringLiteral("textile")},
        {QStringLiteral("image"),    QStringLiteral("图像"),
         QStringLiteral("PNG 图像 (*.png);;JPEG 图像 (*.jpg *.jpeg)"), QStringLiteral("png")},
        {QStringLiteral("markdown"), QStringLiteral("Markdown"),
         QStringLiteral("Markdown 文件 (*.md *.markdown)"), QStringLiteral("md")},
        {QStringLiteral("plaintext"),QStringLiteral("纯文本"),
         QStringLiteral("纯文本 (*.txt)"), QStringLiteral("txt")},
    };
}

Exporter::Format Exporter::formatById(const QString &id)
{
    for (const Format &f : formats()) {
        if (f.id == id) {
            return f;
        }
    }
    return {};
}

// ===========================================================================
// 转义辅助
// ===========================================================================

namespace {

QString escXml(const QString &s)
{
    QString o = s;
    o.replace(QLatin1Char('&'), QLatin1String("&amp;"));
    o.replace(QLatin1Char('<'), QLatin1String("&lt;"));
    o.replace(QLatin1Char('>'), QLatin1String("&gt;"));
    o.replace(QLatin1Char('"'), QLatin1String("&quot;"));
    o.replace(QLatin1Char('\''), QLatin1String("&apos;"));
    return o;
}

QString escHtml(const QString &s)
{
    return escXml(s);
}

QString escLatex(const QString &s)
{
    QString o;
    o.reserve(s.size() * 2);
    for (const QChar c : s) {
        switch (c.unicode()) {
        case '\\': o += QLatin1String("\\textbackslash{}"); break;
        case '&':  o += QLatin1String("\\&"); break;
        case '%':  o += QLatin1String("\\%"); break;
        case '$':  o += QLatin1String("\\$"); break;
        case '#':  o += QLatin1String("\\#"); break;
        case '_':  o += QLatin1String("\\_"); break;
        case '{':  o += QLatin1String("\\{"); break;
        case '}':  o += QLatin1String("\\}"); break;
        case '~':  o += QLatin1String("\\textasciitilde{}"); break;
        case '^':  o += QLatin1String("\\textasciicircum{}"); break;
        default:   o += c; break;
        }
    }
    return o;
}

// RTF：非 ASCII 一律转成 \uN?，避免依赖代码页
QString escRtf(const QString &s)
{
    QString o;
    o.reserve(s.size() * 6);
    for (const QChar c : s) {
        const ushort u = c.unicode();
        if (u == '\\') {
            o += QLatin1String("\\\\");
        } else if (u == '{') {
            o += QLatin1String("\\{");
        } else if (u == '}') {
            o += QLatin1String("\\}");
        } else if (u == '\n') {
            o += QLatin1String("\\line ");
        } else if (u < 128) {
            o += c;
        } else {
            // RTF 的 \uN 用有符号 16 位
            o += QStringLiteral("\\u%1?").arg(static_cast<short>(u));
        }
    }
    return o;
}

// 行内 -> 各格式的纯文本（保留格式标记由各生成器处理）
QString inlinesPlain(const QList<DocInline> &inlines)
{
    QString s;
    for (const DocInline &i : inlines) {
        s += i.text;
    }
    return s;
}

}  // namespace

// ===========================================================================
// Markdown（规范化输出，非原样）
// ===========================================================================

QByteArray Exporter::toMarkdown(const QString &markdown)
{
    const QList<DocBlock> blocks = DocModel::parse(markdown);
    QStringList out;
    bool lastWasList = false;

    for (const DocBlock &b : blocks) {
        // 列表项之间不空行，但列表块结束后要补一个空行，
        // 否则「- 项」紧跟「## 标题」会被某些渲染器当成列表内容。
        if (lastWasList && b.type != DocBlock::ListItem) {
            out << QString();
            lastWasList = false;
        }
        switch (b.type) {
        case DocBlock::Heading:
            out << QString(b.level, QLatin1Char('#')) + QLatin1Char(' ')
                       + inlinesPlain(b.inlines);
            out << QString();
            break;
        case DocBlock::Paragraph: {
            // 从 IR 还原行内标记
            QString line;
            for (const DocInline &i : b.inlines) {
                QString t = i.text;
                if (!i.imageSrc.isEmpty()) {
                    line += QStringLiteral("![%1](%2)").arg(i.imageAlt, i.imageSrc);
                    continue;
                }
                if (i.code) t = QLatin1Char('`') + t + QLatin1Char('`');
                if (i.math) t = QLatin1Char('$') + t + QLatin1Char('$');
                if (i.highlight) t = QStringLiteral("==") + t + QStringLiteral("==");
                if (i.strike) t = QStringLiteral("~~") + t + QStringLiteral("~~");
                if (i.bold && i.italic) t = QStringLiteral("***") + t + QStringLiteral("***");
                else if (i.bold) t = QStringLiteral("**") + t + QStringLiteral("**");
                else if (i.italic) t = QLatin1Char('*') + t + QLatin1Char('*');
                if (i.superscript) t = QLatin1Char('^') + t + QLatin1Char('^');
                if (i.subscript) t = QLatin1Char('~') + t + QLatin1Char('~');
                if (!i.link.isEmpty()) t = QLatin1Char('[') + t + QStringLiteral("](") + i.link + QLatin1Char(')');
                line += t;
            }
            out << line << QString();
            break;
        }
        case DocBlock::ListItem: {
            QString prefix;
            if (b.isTask) {
                prefix = b.checked ? QStringLiteral("- [x] ") : QStringLiteral("- [ ] ");
            } else if (b.ordered) {
                prefix = QStringLiteral("1. ");
            } else {
                prefix = QStringLiteral("- ");
            }
            out << QString(b.level * 2, QLatin1Char(' ')) + prefix + inlinesPlain(b.inlines);
            lastWasList = true;
            break;
        }
        case DocBlock::Quote:
            out << QStringLiteral("> ") + inlinesPlain(b.inlines) << QString();
            break;
        case DocBlock::CodeBlock:
            out << QStringLiteral("```") + b.lang;
            out << b.codeLines;
            out << QStringLiteral("```") << QString();
            break;
        case DocBlock::MathBlock:
            out << QStringLiteral("$$");
            out << b.codeLines;
            out << QStringLiteral("$$") << QString();
            break;
        case DocBlock::Table: {
            if (b.tableRows.isEmpty()) break;
            out << QStringLiteral("| ") + b.tableRows.at(0).join(QStringLiteral(" | ")) + QStringLiteral(" |");
            QStringList sep;
            for (int i = 0; i < b.tableRows.at(0).size(); ++i) {
                const QString a = (i < b.tableAligns.size()) ? b.tableAligns.at(i) : QString();
                if (a == QLatin1String("center")) sep << QStringLiteral(":---:");
                else if (a == QLatin1String("right")) sep << QStringLiteral("---:");
                else if (a == QLatin1String("left")) sep << QStringLiteral(":---");
                else sep << QStringLiteral("---");
            }
            out << QStringLiteral("| ") + sep.join(QStringLiteral(" | ")) + QStringLiteral(" |");
            for (int r = 1; r < b.tableRows.size(); ++r) {
                out << QStringLiteral("| ") + b.tableRows.at(r).join(QStringLiteral(" | ")) + QStringLiteral(" |");
            }
            out << QString();
            break;
        }
        case DocBlock::HorizontalRule:
            out << QStringLiteral("---") << QString();
            break;
        case DocBlock::Toc:
            out << QStringLiteral("[TOC]") << QString();
            break;
        case DocBlock::RawHtml:
            out << b.raw << QString();
            break;
        }
    }

    QString text = out.join(QLatin1Char('\n'));
    // 压掉连续 3 个以上空行
    text.replace(QRegularExpression(QStringLiteral("\n{3,}")), QStringLiteral("\n\n"));
    return text.toUtf8();
}

// ===========================================================================
// 纯文本
// ===========================================================================

QByteArray Exporter::toPlainText(const QString &markdown)
{
    const QList<DocBlock> blocks = DocModel::parse(markdown);
    return DocModel::toPlainText(blocks).toUtf8();
}

// ===========================================================================
// LaTeX
// ===========================================================================

QByteArray Exporter::toLatex(const QString &markdown)
{
    const QList<DocBlock> blocks = DocModel::parse(markdown);
    QStringList out;

    out << QStringLiteral("\\documentclass[12pt]{article}");
    out << QStringLiteral("\\usepackage[utf8]{inputenc}");
    out << QStringLiteral("\\usepackage[T1]{fontenc}");
    out << QStringLiteral("\\usepackage{ctex}");
    out << QStringLiteral("\\usepackage{hyperref}");
    out << QStringLiteral("\\usepackage{graphicx}");
    out << QStringLiteral("\\usepackage{amsmath}");
    out << QStringLiteral("\\usepackage{ulem}");
    out << QStringLiteral("\\usepackage{xcolor}");
    out << QStringLiteral("\\usepackage{listings}");
    out << QStringLiteral("\\lstset{basicstyle=\\ttfamily\\small,breaklines=true,frame=single}");
    const QString docTitle = DocModel::documentTitle(blocks);
    out << QStringLiteral("\\title{%1}").arg(escLatex(docTitle));
    out << QStringLiteral("\\date{}");
    out << QString();
    out << QStringLiteral("\\begin{document}");
    out << QStringLiteral("\\maketitle");
    out << QString();

    // 行内转换（闭包，供各处复用）
    auto inlineTex = [](const QList<DocInline> &inlines) {
        QString s;
        for (const DocInline &i : inlines) {
            if (!i.imageSrc.isEmpty()) {
                s += QStringLiteral("\\includegraphics[width=0.8\\linewidth]{%1}").arg(escLatex(i.imageSrc));
                continue;
            }
            QString t;
            if (i.math) {
                t = QStringLiteral("$%1$").arg(i.text);
            } else if (i.code) {
                t = QStringLiteral("\\texttt{%1}").arg(escLatex(i.text));
            } else {
                t = escLatex(i.text);
            }
            if (i.highlight) t = QStringLiteral("\\colorbox{yellow}{%1}").arg(t);
            if (i.strike) t = QStringLiteral("\\sout{%1}").arg(t);
            if (i.bold && i.italic) t = QStringLiteral("\\textbf{\\textit{%1}}").arg(t);
            else if (i.bold) t = QStringLiteral("\\textbf{%1}").arg(t);
            else if (i.italic) t = QStringLiteral("\\textit{%1}").arg(t);
            if (i.superscript) t = QStringLiteral("\\textsuperscript{%1}").arg(t);
            if (i.subscript) t = QStringLiteral("\\textsubscript{%1}").arg(t);
            if (!i.link.isEmpty()) {
                t = QStringLiteral("\\href{%1}{%2}").arg(escLatex(i.link), t);
            }
            s += t;
        }
        return s;
    };

    // 列表环境需要连续项合并
    QString listEnv;
    bool titleEmitted = false;
    for (const DocBlock &b : blocks) {
        switch (b.type) {
        case DocBlock::Heading: {
            static const char *cmds[] = {"section", "subsection", "subsubsection",
                                         "paragraph", "subparagraph", "subparagraph"};
            const int lv = qBound(1, b.level, 6);
            if (!listEnv.isEmpty()) {
                out << QStringLiteral("\\end{%1}").arg(listEnv);
                listEnv.clear();
            }
            // 首个 H1 已经作为 \title + \maketitle 输出，不重复成 section
            if (lv == 1 && !titleEmitted && inlineTex(b.inlines) == escLatex(docTitle)) {
                titleEmitted = true;
                break;
            }
            if (lv == 1) {
                titleEmitted = true;
            }
            out << QStringLiteral("\\%1{%2}")
                       .arg(QString::fromLatin1(cmds[lv - 1]), inlineTex(b.inlines));
            out << QString();
            break;
        }
        case DocBlock::Paragraph: {
            if (!listEnv.isEmpty()) {
                out << QStringLiteral("\\end{%1}").arg(listEnv);
                listEnv.clear();
            }
            // [TOC] 语法 -> LaTeX 目录命令
            if (inlinesPlain(b.inlines).trimmed() == QLatin1String("[TOC]")) {
                out << QStringLiteral("\\tableofcontents");
                out << QStringLiteral("\\newpage");
                out << QString();
                break;
            }
            out << inlineTex(b.inlines);
            out << QString();
            break;
        }
        case DocBlock::ListItem: {
            const QString want = b.ordered ? QStringLiteral("enumerate")
                                           : QStringLiteral("itemize");
            if (listEnv != want) {
                if (!listEnv.isEmpty()) {
                    out << QStringLiteral("\\end{%1}").arg(listEnv);
                }
                out << QStringLiteral("\\begin{%1}").arg(want);
                listEnv = want;
            }
            QString mark = b.isTask ? (b.checked ? QStringLiteral("$\\boxtimes$ ")
                                                 : QStringLiteral("$\\square$ "))
                                    : QString();
            out << QStringLiteral("\\item %1%2").arg(mark, inlineTex(b.inlines));
            break;
        }
        case DocBlock::Quote:
            if (!listEnv.isEmpty()) {
                out << QStringLiteral("\\end{%1}").arg(listEnv);
                listEnv.clear();
            }
            out << QStringLiteral("\\begin{quote}");
            out << inlineTex(b.inlines);
            out << QStringLiteral("\\end{quote}");
            out << QString();
            break;
        case DocBlock::CodeBlock:
            if (!listEnv.isEmpty()) {
                out << QStringLiteral("\\end{%1}").arg(listEnv);
                listEnv.clear();
            }
            out << QStringLiteral("\\begin{lstlisting}");
            out << b.codeLines.join(QLatin1Char('\n'));
            out << QStringLiteral("\\end{lstlisting}");
            out << QString();
            break;
        case DocBlock::MathBlock: {
            if (!listEnv.isEmpty()) {
                out << QStringLiteral("\\end{%1}").arg(listEnv);
                listEnv.clear();
            }
            out << QStringLiteral("\\begin{equation*}");
            out << b.codeLines.join(QLatin1Char('\n'));
            out << QStringLiteral("\\end{equation*}");
            out << QString();
            break;
        }
        case DocBlock::Table: {
            if (!listEnv.isEmpty()) {
                out << QStringLiteral("\\end{%1}").arg(listEnv);
                listEnv.clear();
            }
            if (b.tableRows.isEmpty()) break;
            const int cols = b.tableRows.at(0).size();
            out << QStringLiteral("\\begin{tabular}{%1}")
                       .arg(QString(cols, QLatin1Char('l')));
            out << QStringLiteral("\\hline");
            for (const QStringList &row : b.tableRows) {
                QStringList cells;
                for (const QString &c : row) {
                    cells << escLatex(c);
                }
                out << cells.join(QStringLiteral(" & ")) + QStringLiteral(" \\\\");
                out << QStringLiteral("\\hline");
            }
            out << QStringLiteral("\\end{tabular}");
            out << QString();
            break;
        }
        case DocBlock::HorizontalRule:
            if (!listEnv.isEmpty()) {
                out << QStringLiteral("\\end{%1}").arg(listEnv);
                listEnv.clear();
            }
            out << QStringLiteral("\\vspace{1em}\\hrule\\vspace{1em}");
            out << QString();
            break;
        case DocBlock::Toc:
            break;
        case DocBlock::RawHtml:
            break;
        }
    }
    if (!listEnv.isEmpty()) {
        out << QStringLiteral("\\end{%1}").arg(listEnv);
    }

    out << QString();
    out << QStringLiteral("\\end{document}");

    return out.join(QLatin1Char('\n')).toUtf8();
}

// ===========================================================================
// RTF
// ===========================================================================

QByteArray Exporter::toRtf(const QString &markdown)
{
    const QList<DocBlock> blocks = DocModel::parse(markdown);

    QString body;

    auto inlineRtf = [](const QList<DocInline> &inlines) {
        QString s;
        for (const DocInline &i : inlines) {
            QString t = escRtf(i.text);
            if (!i.imageSrc.isEmpty()) {
                // RTF 内嵌图片需要 pict 组，这里降级为带说明的占位文本
                s += QStringLiteral("[图片: %1]").arg(escRtf(i.imageAlt));
                continue;
            }
            if (i.code) {
                s += QStringLiteral("{\\f1\\cf2 ") + t + QStringLiteral("}");
                continue;
            }
            if (i.highlight) t = QStringLiteral("{\\highlight1 ") + t + QStringLiteral("}");
            if (i.strike) t = QStringLiteral("{\\strike ") + t + QStringLiteral("}");
            if (i.bold) t = QStringLiteral("{\\b ") + t + QStringLiteral("}");
            if (i.italic) t = QStringLiteral("{\\i ") + t + QStringLiteral("}");
            if (i.superscript) t = QStringLiteral("{\\super ") + t + QStringLiteral("}");
            if (i.subscript) t = QStringLiteral("{\\sub ") + t + QStringLiteral("}");
            if (i.underline) t = QStringLiteral("{\\ul ") + t + QStringLiteral("}");
            if (!i.link.isEmpty()) {
                t = QStringLiteral("{\\field{\\*\\fldinst HYPERLINK \"%1\"}{\\fldrslt %2}}")
                        .arg(escRtf(i.link), t);
            }
            s += t;
        }
        return s;
    };

    for (const DocBlock &b : blocks) {
        switch (b.type) {
        case DocBlock::Heading: {
            static const int sizes[] = {36, 32, 28, 24, 22, 20};
            const int idx = qBound(1, b.level, 6) - 1;
            body += QStringLiteral("\\pard\\sb240\\sa120\\b\\fs%1 %2\\b0\\fs24\\par\n")
                        .arg(sizes[idx])
                        .arg(inlineRtf(b.inlines));
            break;
        }
        case DocBlock::Paragraph:
            body += QStringLiteral("\\pard\\sa120\\fs24 %1\\par\n").arg(inlineRtf(b.inlines));
            break;
        case DocBlock::ListItem: {
            const QString indent = QString(b.level * 360 + 360, QLatin1Char(' '));
            Q_UNUSED(indent);
            const QString bullet = b.isTask ? (b.checked ? QStringLiteral("\\u9745? ")
                                                         : QStringLiteral("\\u9744? "))
                                            : (b.ordered ? QStringLiteral("\\u8226? ")
                                                         : QStringLiteral("\\u8226? "));
            body += QStringLiteral("\\pard\\li%1\\fi-360\\sa60\\fs24 %2%3\\par\n")
                        .arg(b.level * 360 + 360)
                        .arg(bullet, inlineRtf(b.inlines));
            break;
        }
        case DocBlock::Quote: {
            const QString inner = inlineRtf(b.inlines);
            body += QStringLiteral("\\pard\\li720\\ri720\\sa120\\fs24\\i %1\\i0\\par\n").arg(inner);
            break;
        }
        case DocBlock::CodeBlock: {
            body += QStringLiteral("\\pard\\sa120\\f1\\fs20\\cb2\n");
            for (const QString &l : b.codeLines) {
                body += escRtf(l) + QStringLiteral("\\line\n");
            }
            body += QStringLiteral("\\pard\\f0\\fs24\n");
            break;
        }
        case DocBlock::MathBlock:
            body += QStringLiteral("\\pard\\sa120\\f1\\fs20 %1\\par\n")
                        .arg(escRtf(b.codeLines.join(QLatin1Char('\n'))));
            break;
        case DocBlock::Table: {
            if (b.tableRows.isEmpty()) break;
            const int cols = b.tableRows.at(0).size();
            for (const QStringList &row : b.tableRows) {
                QStringList cells;
                for (const QString &c : row) {
                    cells << escRtf(c);
                }
                body += QStringLiteral("\\trowd\\trgaph80");
                for (int c = 1; c <= cols; ++c) {
                    body += QStringLiteral("\\cellx%1").arg(c * 2000);
                }
                for (const QString &c : cells) {
                    body += QStringLiteral("\\pard\\intbl %1\\cell").arg(c);
                }
                body += QStringLiteral("\\row\n");
            }
            break;
        }
        case DocBlock::HorizontalRule:
            body += QStringLiteral("\\pard\\brdrb\\brdrs\\brdrw10\\brsp20 \\par\n");
            break;
        case DocBlock::Toc:
            break;
        case DocBlock::RawHtml:
            break;
        }
    }

    QString rtf;
    rtf += QStringLiteral("{\\rtf1\\ansi\\ansicpg936\\deff0\\uc1\n");
    rtf += QStringLiteral("{\\fonttbl{\\f0\\fnil\\fcharset134 Microsoft YaHei;}"
                          "{\\f1\\fmodern\\fcharset0 Consolas;}}\n");
    rtf += QStringLiteral("{\\colortbl;\\red0\\green0\\blue0;\\red200\\green40\\blue40;"
                          "\\red245\\green245\\blue245;}\n");
    rtf += QStringLiteral("{\\*\\generator Markdown Editor;}\n");
    rtf += body;
    rtf += QStringLiteral("}\n");

    return rtf.toUtf8();
}

// ===========================================================================
// OPML（大纲）
// ===========================================================================

QByteArray Exporter::toOpml(const QString &markdown)
{
    const QList<DocBlock> blocks = DocModel::parse(markdown);

    QString out;
    out += QStringLiteral("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
    out += QStringLiteral("<opml version=\"2.0\">\n");
    out += QStringLiteral("  <head>\n");
    out += QStringLiteral("    <title>%1</title>\n").arg(escXml(DocModel::documentTitle(blocks)));
    out += QStringLiteral("    <dateCreated>%1</dateCreated>\n")
               .arg(QDateTime::currentDateTime().toString(Qt::RFC2822Date));
    out += QStringLiteral("  </head>\n");
    out += QStringLiteral("  <body>\n");

    // 标题层级 -> 嵌套 outline
    QList<int> levelStack;   // 各层级的 heading level
    QString indent = QStringLiteral("    ");

    for (const DocBlock &b : blocks) {
        if (b.type == DocBlock::Heading) {
            while (!levelStack.isEmpty() && levelStack.last() >= b.level) {
                indent.chop(2);
                out += indent + QStringLiteral("</outline>\n");
                levelStack.removeLast();
            }
            out += QStringLiteral("%1<outline text=\"%2\">\n")
                       .arg(indent, escXml(inlinesPlain(b.inlines)));
            indent += QStringLiteral("  ");
            levelStack.append(b.level);
        } else if (b.type == DocBlock::Paragraph && !levelStack.isEmpty()) {
            out += QStringLiteral("%1<outline text=\"%2\"/>\n")
                       .arg(indent, escXml(inlinesPlain(b.inlines)));
        }
    }
    while (!levelStack.isEmpty()) {
        indent.chop(2);
        out += indent + QStringLiteral("</outline>\n");
        levelStack.removeLast();
    }

    out += QStringLiteral("  </body>\n");
    out += QStringLiteral("</opml>\n");
    return out.toUtf8();
}

// ===========================================================================
// MediaWiki
// ===========================================================================

QByteArray Exporter::toMediaWiki(const QString &markdown)
{
    const QList<DocBlock> blocks = DocModel::parse(markdown);
    QStringList out;

    auto inlineWiki = [](const QList<DocInline> &inlines) {
        QString s;
        for (const DocInline &i : inlines) {
            if (!i.imageSrc.isEmpty()) {
                s += QStringLiteral("[[File:%1|thumb|%2]]").arg(i.imageSrc, i.imageAlt);
                continue;
            }
            QString t = i.text;
            if (i.code) {
                t = QStringLiteral("<code>") + escHtml(t) + QStringLiteral("</code>");
            } else {
                if (i.highlight) t = QStringLiteral("<span style=\"background:yellow\">") + escHtml(t) + QStringLiteral("</span>");
                if (i.strike) t = QStringLiteral("<s>") + t + QStringLiteral("</s>");
                if (i.bold && i.italic) t = QStringLiteral("'''''") + t + QStringLiteral("'''''");
                else if (i.bold) t = QStringLiteral("'''") + t + QStringLiteral("'''");
                else if (i.italic) t = QStringLiteral("''") + t + QStringLiteral("''");
                if (i.superscript) t = QStringLiteral("<sup>") + t + QStringLiteral("</sup>");
                if (i.subscript) t = QStringLiteral("<sub>") + t + QStringLiteral("</sub>");
                if (!i.link.isEmpty()) {
                    t = QStringLiteral("[%1 %2]").arg(escHtml(i.link), t);
                }
            }
            s += t;
        }
        return s;
    };

    for (const DocBlock &b : blocks) {
        switch (b.type) {
        case DocBlock::Heading:
            out << QString(b.level, QLatin1Char('=')) + QLatin1Char(' ')
                       + inlinesPlain(b.inlines) + QLatin1Char(' ')
                       + QString(b.level, QLatin1Char('='));
            out << QString();
            break;
        case DocBlock::Paragraph:
            out << inlineWiki(b.inlines) << QString();
            break;
        case DocBlock::ListItem: {
            const QString prefix = b.isTask
                ? (b.checked ? QStringLiteral("* {{done}} ")
                             : QStringLiteral("* {{not done}} "))
                : (b.ordered ? QStringLiteral("# ") : QStringLiteral("* "));
            out << QString(b.level, QLatin1Char(b.ordered ? '#' : '*')) + QLatin1Char(' ')
                       + inlineWiki(b.inlines);
            break;
        }
        case DocBlock::Quote:
            out << QStringLiteral("<blockquote>") + inlineWiki(b.inlines)
                       + QStringLiteral("</blockquote>");
            out << QString();
            break;
        case DocBlock::CodeBlock:
            out << QStringLiteral("<syntaxhighlight lang=\"%1\">").arg(b.lang);
            out << b.codeLines;
            out << QStringLiteral("</syntaxhighlight>") << QString();
            break;
        case DocBlock::MathBlock:
            out << QStringLiteral("<math>") + b.codeLines.join(QLatin1Char('\n'))
                       + QStringLiteral("</math>");
            out << QString();
            break;
        case DocBlock::Table: {
            if (b.tableRows.isEmpty()) break;
            out << QStringLiteral("{| class=\"wikitable\"");
            QStringList header = b.tableRows.at(0);
            out << QStringLiteral("|+ ") + header.join(QStringLiteral(" !! "));
            for (int r = 1; r < b.tableRows.size(); ++r) {
                out << QStringLiteral("|-");
                out << QStringLiteral("| ") + b.tableRows.at(r).join(QStringLiteral(" || "));
            }
            out << QStringLiteral("|}");
            out << QString();
            break;
        }
        case DocBlock::HorizontalRule:
            out << QStringLiteral("----") << QString();
            break;
        case DocBlock::Toc:
            out << QStringLiteral("[TOC]") << QString();
            break;
        case DocBlock::RawHtml:
            out << b.raw << QString();
            break;
        }
    }
    return out.join(QLatin1Char('\n')).toUtf8();
}

// ===========================================================================
// reStructuredText
// ===========================================================================

QByteArray Exporter::toRst(const QString &markdown)
{
    const QList<DocBlock> blocks = DocModel::parse(markdown);
    QStringList out;
    out << QString();

    static const char *underlineChars = "=-~^\"'";

    auto inlineRst = [](const QList<DocInline> &inlines) {
        QString s;
        for (const DocInline &i : inlines) {
            if (!i.imageSrc.isEmpty()) {
                s += QStringLiteral(".. image:: %1\n   :alt: %2").arg(i.imageSrc, i.imageAlt);
                continue;
            }
            QString t = i.text;
            if (i.code) t = QStringLiteral("``") + t + QStringLiteral("``");
            else {
                if (i.strike) t = QStringLiteral(":strike:`%1`").arg(t);
                if (i.highlight) t = QStringLiteral(":highlight:`%1`").arg(t);
                if (i.bold && i.italic) t = QStringLiteral("***%1***").arg(t);
                else if (i.bold) t = QStringLiteral("**%1**").arg(t);
                else if (i.italic) t = QStringLiteral("*%1*").arg(t);
                if (i.superscript) t = QStringLiteral(":sup:`%1`").arg(t);
                if (i.subscript) t = QStringLiteral(":sub:`%1`").arg(t);
                if (!i.link.isEmpty()) t = QStringLiteral("`%1 <%2>`_").arg(t, i.link);
            }
            s += t;
        }
        return s;
    };

    QString lastListType;
    for (const DocBlock &b : blocks) {
        switch (b.type) {
        case DocBlock::Heading: {
            const QString title = inlinesPlain(b.inlines);
            const QChar uc = underlineChars[qBound(1, b.level, 6) - 1];
            out << title;
            out << QString(title.length(), uc);
            out << QString();
            break;
        }
        case DocBlock::Paragraph:
            out << inlineRst(b.inlines) << QString();
            break;
        case DocBlock::ListItem: {
            const QString marker = b.ordered ? QStringLiteral("#.") : QStringLiteral("-");
            if (!b.isTask) {
                out << QString(b.level * 2, QLatin1Char(' ')) + marker + QLatin1Char(' ')
                           + inlineRst(b.inlines);
            } else {
                const QString box = b.checked ? QStringLiteral("[x]") : QStringLiteral("[ ]");
                out << QString(b.level * 2, QLatin1Char(' ')) + marker + QLatin1Char(' ')
                           + box + QLatin1Char(' ') + inlineRst(b.inlines);
            }
            break;
        }
        case DocBlock::Quote:
            out << QStringLiteral("    ") + inlineRst(b.inlines);
            out << QString();
            break;
        case DocBlock::CodeBlock:
            out << QStringLiteral(".. code-block:: %1")
                       .arg(b.lang.isEmpty() ? QStringLiteral("text") : b.lang);
            out << QString();
            for (const QString &l : b.codeLines) {
                out << QStringLiteral("   ") + l;
            }
            out << QString();
            break;
        case DocBlock::MathBlock:
            out << QStringLiteral(".. math::");
            out << QString();
            for (const QString &l : b.codeLines) {
                out << QStringLiteral("   ") + l;
            }
            out << QString();
            break;
        case DocBlock::Table: {
            if (b.tableRows.isEmpty()) break;
            out << QStringLiteral(".. list-table::");
            out << QStringLiteral("   :header-rows: 1");
            out << QString();
            for (const QStringList &row : b.tableRows) {
                for (const QString &c : row) {
                    out << QStringLiteral("   * - ") + c;
                }
            }
            out << QString();
            break;
        }
        case DocBlock::HorizontalRule:
            out << QStringLiteral("----") << QString();
            break;
        case DocBlock::Toc:
            out << QStringLiteral(".. contents::") << QString();
            break;
        case DocBlock::RawHtml:
            out << QStringLiteral(".. raw:: html") << QString();
            out << QStringLiteral("   ") + b.raw << QString();
            break;
        }
    }
    return out.join(QLatin1Char('\n')).toUtf8();
}

// ===========================================================================
// Textile
// ===========================================================================

QByteArray Exporter::toTextile(const QString &markdown)
{
    const QList<DocBlock> blocks = DocModel::parse(markdown);
    QStringList out;

    auto inlineTextile = [](const QList<DocInline> &inlines) {
        QString s;
        for (const DocInline &i : inlines) {
            if (!i.imageSrc.isEmpty()) {
                s += QStringLiteral("!%1(%2)!").arg(i.imageSrc, i.imageAlt);
                continue;
            }
            QString t = i.text;
            if (i.code) t = QStringLiteral("@%1@").arg(t);
            else {
                if (i.highlight) t = QStringLiteral("%{background:yellow}%1%").arg(t);
                if (i.strike) t = QStringLiteral("-") + t + QStringLiteral("-");
                if (i.bold && i.italic) t = QStringLiteral("*_%1_*").arg(t);
                else if (i.bold) t = QStringLiteral("*%1*").arg(t);
                else if (i.italic) t = QStringLiteral("_%1_").arg(t);
                if (i.superscript) t = QStringLiteral("^%1^").arg(t);
                if (i.subscript) t = QStringLiteral("~%1~").arg(t);
                if (!i.link.isEmpty()) t = QStringLiteral("\"%1\":%2").arg(t, i.link);
            }
            s += t;
        }
        return s;
    };

    for (const DocBlock &b : blocks) {
        switch (b.type) {
        case DocBlock::Heading:
            out << QStringLiteral("h%1. %2").arg(b.level).arg(inlinesPlain(b.inlines));
            out << QString();
            break;
        case DocBlock::Paragraph:
            out << inlineTextile(b.inlines) << QString();
            break;
        case DocBlock::ListItem: {
            const QString marker = b.ordered ? QStringLiteral("#") : QStringLiteral("*");
            QString prefix = QString(b.level + 1, marker.at(0));
            if (b.isTask) {
                prefix += QStringLiteral(" [%1]").arg(b.checked ? QStringLiteral("x") : QStringLiteral(" "));
            }
            out << prefix + QLatin1Char(' ') + inlineTextile(b.inlines);
            break;
        }
        case DocBlock::Quote:
            out << QStringLiteral("bq. ") + inlineTextile(b.inlines);
            out << QString();
            break;
        case DocBlock::CodeBlock:
            out << QStringLiteral("bc.. ") + (b.codeLines.isEmpty() ? QString() : b.codeLines.at(0));
            for (int i = 1; i < b.codeLines.size(); ++i) {
                out << b.codeLines.at(i);
            }
            out << QString();
            break;
        case DocBlock::MathBlock:
            out << QStringLiteral("<notextile>") + b.codeLines.join(QLatin1Char('\n'))
                       + QStringLiteral("</notextile>");
            out << QString();
            break;
        case DocBlock::Table: {
            if (b.tableRows.isEmpty()) break;
            out << QStringLiteral("|_. ") + b.tableRows.at(0).join(QStringLiteral(" |_. ")) + QStringLiteral(" |");
            for (int r = 1; r < b.tableRows.size(); ++r) {
                out << QStringLiteral("| ") + b.tableRows.at(r).join(QStringLiteral(" | ")) + QStringLiteral(" |");
            }
            out << QString();
            break;
        }
        case DocBlock::HorizontalRule:
            out << QStringLiteral("<hr>") << QString();
            break;
        case DocBlock::Toc:
            out << QStringLiteral("[TOC]") << QString();
            break;
        case DocBlock::RawHtml:
            out << b.raw << QString();
            break;
        }
    }
    return out.join(QLatin1Char('\n')).toUtf8();
}

// ===========================================================================
// XHTML（EPUB 正文用，也是 EPUB 与 HTML 的共用渲染）
// ===========================================================================

QByteArray Exporter::toXhtml(const QString &markdown)
{
    const QList<DocBlock> blocks = DocModel::parse(markdown);
    QString body;

    auto inlineXhtml = [](const QList<DocInline> &inlines) {
        QString s;
        for (const DocInline &i : inlines) {
            if (!i.imageSrc.isEmpty()) {
                s += QStringLiteral("<img src=\"%1\" alt=\"%2\"/>")
                         .arg(escXml(i.imageSrc), escXml(i.imageAlt));
                continue;
            }
            QString t = escXml(i.text);
            if (i.math) {
                t = QStringLiteral("<code class=\"math\">%1</code>").arg(t);
            } else if (i.code) {
                t = QStringLiteral("<code>%1</code>").arg(t);
            } else {
                if (i.highlight) t = QStringLiteral("<mark>%1</mark>").arg(t);
                if (i.strike) t = QStringLiteral("<del>%1</del>").arg(t);
                if (i.bold && i.italic) t = QStringLiteral("<strong><em>%1</em></strong>").arg(t);
                else if (i.bold) t = QStringLiteral("<strong>%1</strong>").arg(t);
                else if (i.italic) t = QStringLiteral("<em>%1</em>").arg(t);
                if (i.superscript) t = QStringLiteral("<sup>%1</sup>").arg(t);
                if (i.subscript) t = QStringLiteral("<sub>%1</sub>").arg(t);
                if (!i.link.isEmpty()) {
                    t = QStringLiteral("<a href=\"%1\">%2</a>").arg(escXml(i.link), t);
                }
            }
            s += t;
        }
        return s;
    };

    // listTag 只存标签名（ul / ol），属性另存 —— 否则闭合时会生成
    // </ul class="tasks"> 这种非法 XML（闭合标签不能带属性）
    QString listTag;
    bool listIsTasks = false;
    for (int blockIdx = 0; blockIdx < blocks.size(); ++blockIdx) {
        const DocBlock &b = blocks.at(blockIdx);
        // 列表开闭
        auto closeList = [&]() {
            if (!listTag.isEmpty()) {
                body += QStringLiteral("</%1>\n").arg(listTag);
                listTag.clear();
                listIsTasks = false;
            }
        };

        switch (b.type) {
        case DocBlock::Heading:
            closeList();
            body += QStringLiteral("<h%1 id=\"h%2\">%3</h%1>\n")
                        .arg(b.level)
                        .arg(QString::number(blockIdx))
                        .arg(inlineXhtml(b.inlines));
            break;
        case DocBlock::Paragraph:
            closeList();
            body += QStringLiteral("<p>%1</p>\n").arg(inlineXhtml(b.inlines));
            break;
        case DocBlock::ListItem: {
            // 标签名与属性分开：listTag 只存 "ul"/"ol"，
            // 属性由 listIsTasks 决定，闭合时才不会带上属性。
            const QString tagName = b.ordered ? QStringLiteral("ol") : QStringLiteral("ul");
            const bool wantTasks = b.isTask;
            if (listTag != tagName || listIsTasks != wantTasks) {
                closeList();
                if (wantTasks) {
                    body += QStringLiteral("<%1 class=\"tasks\">\n").arg(tagName);
                } else {
                    body += QStringLiteral("<%1>\n").arg(tagName);
                }
                listTag = tagName;
                listIsTasks = wantTasks;
            }
            QString mark;
            if (b.isTask) {
                mark = QStringLiteral("<span class=\"task\">[%1]</span> ")
                           .arg(b.checked ? QStringLiteral("x") : QStringLiteral(" "));
            }
            body += QStringLiteral("<li>%1%2</li>\n").arg(mark, inlineXhtml(b.inlines));
            break;
        }
        case DocBlock::Quote:
            closeList();
            body += QStringLiteral("<blockquote><p>%1</p></blockquote>\n")
                        .arg(inlineXhtml(b.inlines));
            break;
        case DocBlock::CodeBlock:
            closeList();
            body += QStringLiteral("<pre><code class=\"language-%1\">%2</code></pre>\n")
                        .arg(escXml(b.lang), escXml(b.codeLines.join(QLatin1Char('\n'))));
            break;
        case DocBlock::MathBlock:
            closeList();
            body += QStringLiteral("<pre class=\"math\">%1</pre>\n")
                        .arg(escXml(b.codeLines.join(QLatin1Char('\n'))));
            break;
        case DocBlock::Table:
            closeList();
            if (!b.tableRows.isEmpty()) {
                body += QStringLiteral("<table>\n<thead><tr>");
                for (const QString &c : b.tableRows.at(0)) {
                    body += QStringLiteral("<th>%1</th>").arg(escXml(c));
                }
                body += QStringLiteral("</tr></thead>\n<tbody>\n");
                for (int r = 1; r < b.tableRows.size(); ++r) {
                    body += QStringLiteral("<tr>");
                    for (const QString &c : b.tableRows.at(r)) {
                        body += QStringLiteral("<td>%1</td>").arg(escXml(c));
                    }
                    body += QStringLiteral("</tr>\n");
                }
                body += QStringLiteral("</tbody>\n</table>\n");
            }
            break;
        case DocBlock::HorizontalRule:
            closeList();
            body += QStringLiteral("<hr/>\n");
            break;
        case DocBlock::Toc:
            closeList();
            break;
        case DocBlock::RawHtml:
            closeList();
            body += b.raw + QLatin1Char('\n');
            break;
        }
    }
    if (!listTag.isEmpty()) {
        body += QStringLiteral("</%1>\n").arg(listTag);
    }

    return body.toUtf8();
}
