#include "exporter.h"

#include <QDateTime>
#include <QRegularExpression>
#include <QUuid>

#include "zipwriter.h"

// ===========================================================================
// 共用辅助
// ===========================================================================

namespace {

QString xesc(const QString &s)
{
    QString o = s;
    o.replace(QLatin1Char('&'), QLatin1String("&amp;"));
    o.replace(QLatin1Char('<'), QLatin1String("&lt;"));
    o.replace(QLatin1Char('>'), QLatin1String("&gt;"));
    o.replace(QLatin1Char('"'), QLatin1String("&quot;"));
    o.replace(QLatin1Char('\''), QLatin1String("&apos;"));
    return o;
}

QString plainOf(const QList<DocInline> &inlines)
{
    QString s;
    for (const DocInline &i : inlines) {
        s += i.text;
    }
    return s;
}

// ===========================================================================
// OOXML（docx）
// ===========================================================================

/**
 * 生成 word/document.xml。
 *
 * @param hyperlinks 输出：所有外链 URL（按出现顺序），供 rels 使用
 */
QString buildDocumentXml(const QList<DocBlock> &blocks, QStringList &hyperlinks)
{
    QString x;
    x += QStringLiteral(
        "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
        "<w:document "
        "xmlns:w=\"http://schemas.openxmlformats.org/wordprocessingml/2006/main\" "
        "xmlns:r=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships\">\n"
        "<w:body>\n");

    // ---- 行内 run ----
    auto inlineRuns = [&hyperlinks](const QList<DocInline> &inlines) -> QString {
        QString out;
        for (const DocInline &i : inlines) {
            if (!i.imageSrc.isEmpty()) {
                // docx 内嵌图片需要 drawing + media 关系，这里降级为占位文本，
                // 避免生成无法被 Word 打开的半成品结构。
                out += QStringLiteral("<w:r><w:rPr><w:i/><w:color w:val=\"808080\"/></w:rPr>"
                                      "<w:t xml:space=\"preserve\">[图片: %1]</w:t></w:r>")
                           .arg(xesc(i.imageAlt));
                continue;
            }

            QString rPr;
            if (i.bold)        rPr += QStringLiteral("<w:b/>");
            if (i.italic)      rPr += QStringLiteral("<w:i/>");
            if (i.strike)      rPr += QStringLiteral("<w:strike/>");
            if (i.underline)   rPr += QStringLiteral("<w:u w:val=\"single\"/>");
            if (i.code) {
                rPr += QStringLiteral("<w:rFonts w:ascii=\"Consolas\" w:hAnsi=\"Consolas\"/>");
                rPr += QStringLiteral("<w:shd w:val=\"clear\" w:fill=\"F2F2F2\"/>");
            }
            if (i.highlight) {
                rPr += QStringLiteral("<w:highlight w:val=\"yellow\"/>");
            }
            if (i.superscript) {
                rPr += QStringLiteral("<w:vertAlign w:val=\"superscript\"/>");
            }
            if (i.subscript) {
                rPr += QStringLiteral("<w:vertAlign w:val=\"subscript\"/>");
            }
            if (!i.link.isEmpty()) {
                rPr += QStringLiteral("<w:rStyle w:val=\"Hyperlink\"/>");
            } else if (i.math) {
                rPr += QStringLiteral("<w:rFonts w:ascii=\"Cambria Math\" w:hAnsi=\"Cambria Math\"/>");
            }

            const QString runPr = rPr.isEmpty() ? QString()
                                                : QStringLiteral("<w:rPr>") + rPr + QStringLiteral("</w:rPr>");
            const QString text = QStringLiteral("<w:t xml:space=\"preserve\">%1</w:t>").arg(xesc(i.text));
            const QString run = QStringLiteral("<w:r>%1%2</w:r>").arg(runPr, text);

            if (!i.link.isEmpty()) {
                hyperlinks.append(i.link);
                out += QStringLiteral("<w:hyperlink r:id=\"rIdLink%1\">%2</w:hyperlink>")
                           .arg(hyperlinks.size()).arg(run);
            } else {
                out += run;
            }
        }
        return out;
    };

    // ---- 块级 ----
    for (const DocBlock &b : blocks) {
        switch (b.type) {
        case DocBlock::Heading: {
            const int lv = qBound(1, b.level, 6);
            x += QStringLiteral("<w:p><w:pPr><w:pStyle w:val=\"Heading%1\"/>"
                                "<w:spacing w:before=\"240\" w:after=\"120\"/></w:pPr>%2</w:p>\n")
                     .arg(lv).arg(inlineRuns(b.inlines));
            break;
        }
        case DocBlock::Paragraph:
            x += QStringLiteral("<w:p><w:pPr><w:spacing w:after=\"120\"/></w:pPr>%1</w:p>\n")
                     .arg(inlineRuns(b.inlines));
            break;
        case DocBlock::ListItem: {
            const QString numPr = QStringLiteral(
                "<w:numPr><w:ilvl w:val=\"%1\"/><w:numId w:val=\"%2\"/></w:numPr>")
                .arg(qBound(0, b.level, 8))
                .arg(b.ordered ? 1 : 2);
            QString mark;
            if (b.isTask) {
                mark = QStringLiteral("<w:r><w:t xml:space=\"preserve\">%1</w:t></w:r>")
                           .arg(b.checked ? QStringLiteral("☑ ") : QStringLiteral("☐ "));
            }
            x += QStringLiteral("<w:p><w:pPr>%1<w:spacing w:after=\"60\"/></w:pPr>%2%3</w:p>\n")
                     .arg(numPr, mark, inlineRuns(b.inlines));
            break;
        }
        case DocBlock::Quote:
            x += QStringLiteral("<w:p><w:pPr><w:pStyle w:val=\"Quote\"/>"
                                "<w:ind w:left=\"720\"/><w:spacing w:after=\"120\"/></w:pPr>%1</w:p>\n")
                     .arg(inlineRuns(b.inlines));
            break;
        case DocBlock::CodeBlock: {
            x += QStringLiteral("<w:p><w:pPr><w:pStyle w:val=\"CodeBlock\"/>"
                                "<w:shd w:val=\"clear\" w:fill=\"F7F7F7\"/></w:pPr>");
            const QString code = b.codeLines.join(QLatin1Char('\n'));
            x += QStringLiteral("<w:r><w:rPr><w:rFonts w:ascii=\"Consolas\" w:hAnsi=\"Consolas\"/>"
                                "<w:sz w:val=\"20\"/></w:rPr>"
                                "<w:t xml:space=\"preserve\">%1</w:t></w:r></w:p>\n")
                     .arg(xesc(code));
            break;
        }
        case DocBlock::MathBlock:
            x += QStringLiteral("<w:p><w:pPr><w:pStyle w:val=\"CodeBlock\"/></w:pPr>"
                                "<w:r><w:rPr><w:rFonts w:ascii=\"Cambria Math\" w:hAnsi=\"Cambria Math\"/></w:rPr>"
                                "<w:t xml:space=\"preserve\">%1</w:t></w:r></w:p>\n")
                     .arg(xesc(b.codeLines.join(QLatin1Char('\n'))));
            break;
        case DocBlock::Table: {
            if (b.tableRows.isEmpty()) break;
            const int cols = b.tableRows.at(0).size();
            x += QStringLiteral("<w:tbl><w:tblPr><w:tblStyle w:val=\"TableGrid\"/>"
                                "<w:tblW w:w=\"0\" w:type=\"auto\"/>"
                                "<w:tblBorders>"
                                "<w:top w:val=\"single\" w:sz=\"4\" w:color=\"BFBFBF\"/>"
                                "<w:left w:val=\"single\" w:sz=\"4\" w:color=\"BFBFBF\"/>"
                                "<w:bottom w:val=\"single\" w:sz=\"4\" w:color=\"BFBFBF\"/>"
                                "<w:right w:val=\"single\" w:sz=\"4\" w:color=\"BFBFBF\"/>"
                                "<w:insideH w:val=\"single\" w:sz=\"4\" w:color=\"BFBFBF\"/>"
                                "<w:insideV w:val=\"single\" w:sz=\"4\" w:color=\"BFBFBF\"/>"
                                "</w:tblBorders></w:tblPr>\n");
            x += QStringLiteral("<w:tblGrid>");
            for (int c = 0; c < cols; ++c) {
                x += QStringLiteral("<w:gridCol w:w=\"%1\"/>").arg(9000 / qMax(1, cols));
            }
            x += QStringLiteral("</w:tblGrid>\n");

            for (int r = 0; r < b.tableRows.size(); ++r) {
                const bool header = (r == 0);
                x += QStringLiteral("<w:tr>");
                for (const QString &cell : b.tableRows.at(r)) {
                    x += QStringLiteral("<w:tc><w:tcPr>");
                    if (header) {
                        x += QStringLiteral("<w:shd w:val=\"clear\" w:fill=\"F2F2F2\"/>");
                    }
                    x += QStringLiteral("</w:tcPr><w:p><w:pPr><w:spacing w:after=\"0\"/></w:pPr>");
                    QString rPr;
                    if (header) {
                        rPr = QStringLiteral("<w:rPr><w:b/></w:rPr>");
                    }
                    x += QStringLiteral("<w:r>%1<w:t xml:space=\"preserve\">%2</w:t></w:r>")
                             .arg(rPr, xesc(cell));
                    x += QStringLiteral("</w:p></w:tc>");
                }
                x += QStringLiteral("</w:tr>\n");
            }
            x += QStringLiteral("</w:tbl>\n");
            // 表后补一个空段落，避免两表相连
            x += QStringLiteral("<w:p/>\n");
            break;
        }
        case DocBlock::HorizontalRule:
            x += QStringLiteral("<w:p><w:pPr><w:pBdr>"
                                "<w:bottom w:val=\"single\" w:sz=\"6\" w:color=\"BFBFBF\"/>"
                                "</w:pBdr></w:pPr></w:p>\n");
            break;
        case DocBlock::Toc:
            break;
        case DocBlock::RawHtml:
            break;
        }
    }

    // ---- 节属性（A4 纵向） ----
    x += QStringLiteral(
        "<w:sectPr>"
        "<w:pgSz w:w=\"11906\" w:h=\"16838\"/>"
        "<w:pgMar w:top=\"1440\" w:right=\"1440\" w:bottom=\"1440\" w:left=\"1440\" "
        "w:header=\"851\" w:footer=\"992\" w:gutter=\"0\"/>"
        "</w:sectPr>\n");
    x += QStringLiteral("</w:body>\n</w:document>\n");
    return x;
}

// docx 的 styles.xml（定义 Heading1-6 / Normal / Quote / CodeBlock / Hyperlink）
QString buildStylesXml()
{
    QString s;
    s += QStringLiteral(
        "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
        "<w:styles xmlns:w=\"http://schemas.openxmlformats.org/wordprocessingml/2006/main\">\n");

    // 默认字体
    s += QStringLiteral(
        "<w:docDefaults><w:rPrDefault><w:rPr>"
        "<w:rFonts w:ascii=\"Microsoft YaHei\" w:hAnsi=\"Microsoft YaHei\" "
        "w:eastAsia=\"Microsoft YaHei\" w:cs=\"Microsoft YaHei\"/>"
        "<w:sz w:val=\"22\"/><w:szCs w:val=\"22\"/>"
        "</w:rPr></w:rPrDefault>"
        "<w:pPrDefault><w:pPr><w:spacing w:after=\"120\" w:line=\"300\" w:lineRule=\"auto\"/>"
        "</w:pPr></w:pPrDefault></w:docDefaults>\n");

    // Normal
    s += QStringLiteral(
        "<w:style w:type=\"paragraph\" w:default=\"1\" w:styleId=\"Normal\">"
        "<w:name w:val=\"Normal\"/><w:qFormat/></w:style>\n");

    // Heading1-6
    static const int sizes[] = {40, 34, 30, 26, 24, 22};
    for (int i = 1; i <= 6; ++i) {
        s += QStringLiteral(
            "<w:style w:type=\"paragraph\" w:styleId=\"Heading%1\">"
            "<w:name w:val=\"heading %1\"/><w:basedOn w:val=\"Normal\"/>"
            "<w:next w:val=\"Normal\"/><w:qFormat/>"
            "<w:pPr><w:keepNext/><w:outlineLvl w:val=\"%2\"/></w:pPr>"
            "<w:rPr><w:b/><w:sz w:val=\"%3\"/><w:szCs w:val=\"%3\"/></w:rPr>"
            "</w:style>\n").arg(i).arg(i - 1).arg(sizes[i - 1]);
    }

    // Quote
    s += QStringLiteral(
        "<w:style w:type=\"paragraph\" w:styleId=\"Quote\">"
        "<w:name w:val=\"Quote\"/><w:basedOn w:val=\"Normal\"/>"
        "<w:rPr><w:i/><w:color w:val=\"666666\"/></w:rPr></w:style>\n");

    // CodeBlock
    s += QStringLiteral(
        "<w:style w:type=\"paragraph\" w:styleId=\"CodeBlock\">"
        "<w:name w:val=\"Code Block\"/><w:basedOn w:val=\"Normal\"/>"
        "<w:rPr><w:rFonts w:ascii=\"Consolas\" w:hAnsi=\"Consolas\"/>"
        "<w:sz w:val=\"20\"/></w:rPr></w:style>\n");

    // Hyperlink（字符样式）
    s += QStringLiteral(
        "<w:style w:type=\"character\" w:styleId=\"Hyperlink\">"
        "<w:name w:val=\"Hyperlink\"/>"
        "<w:rPr><w:color w:val=\"0563C1\"/><w:u w:val=\"single\"/></w:rPr></w:style>\n");

    s += QStringLiteral("</w:styles>\n");
    return s;
}

QString buildContentTypesXml()
{
    return QStringLiteral(
        "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
        "<Types xmlns=\"http://schemas.openxmlformats.org/package/2006/content-types\">"
        "<Default Extension=\"rels\" ContentType=\"application/vnd.openxmlformats-package.relationships+xml\"/>"
        "<Default Extension=\"xml\" ContentType=\"application/xml\"/>"
        "<Override PartName=\"/word/document.xml\" "
        "ContentType=\"application/vnd.openxmlformats-officedocument.wordprocessingml.document.main+xml\"/>"
        "<Override PartName=\"/word/styles.xml\" "
        "ContentType=\"application/vnd.openxmlformats-officedocument.wordprocessingml.styles+xml\"/>"
        "<Override PartName=\"/word/numbering.xml\" "
        "ContentType=\"application/vnd.openxmlformats-officedocument.wordprocessingml.numbering+xml\"/>"
        "<Override PartName=\"/docProps/core.xml\" "
        "ContentType=\"application/vnd.openxmlformats-package.core-properties+xml\"/>"
        "<Override PartName=\"/docProps/app.xml\" "
        "ContentType=\"application/vnd.openxmlformats-officedocument.extended-properties+xml\"/>"
        "</Types>\n");
}

QString buildRelsXml()
{
    return QStringLiteral(
        "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
        "<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">"
        "<Relationship Id=\"rId1\" "
        "Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/officeDocument\" "
        "Target=\"word/document.xml\"/>"
        "<Relationship Id=\"rId2\" "
        "Type=\"http://schemas.openxmlformats.org/package/2006/relationships/metadata/core-properties\" "
        "Target=\"docProps/core.xml\"/>"
        "<Relationship Id=\"rId3\" "
        "Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/extended-properties\" "
        "Target=\"docProps/app.xml\"/>"
        "</Relationships>\n");
}

QString buildDocumentRelsXml(const QStringList &hyperlinks)
{
    QString s;
    s += QStringLiteral(
        "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
        "<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">"
        "<Relationship Id=\"rIdStyles\" "
        "Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/styles\" "
        "Target=\"styles.xml\"/>"
        "<Relationship Id=\"rIdNumbering\" "
        "Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/numbering\" "
        "Target=\"numbering.xml\"/>");
    for (int i = 0; i < hyperlinks.size(); ++i) {
        s += QStringLiteral(
            "<Relationship Id=\"rIdLink%1\" "
            "Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/hyperlink\" "
            "Target=\"%2\" TargetMode=\"External\"/>")
            .arg(i + 1).arg(xesc(hyperlinks.at(i)));
    }
    s += QStringLiteral("</Relationships>\n");
    return s;
}

QString buildNumberingXml()
{
    // numId=1 有序，numId=2 无序
    return QStringLiteral(
        "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
        "<w:numbering xmlns:w=\"http://schemas.openxmlformats.org/wordprocessingml/2006/main\">"
        // 有序列表定义
        "<w:abstractNum w:abstractNumId=\"0\">"
        "<w:multiLevelType w:val=\"hybridMultilevel\"/>"
        "<w:lvl w:ilvl=\"0\"><w:start w:val=\"1\"/><w:numFmt w:val=\"decimal\"/>"
        "<w:lvlText w:val=\"%1.\"/><w:lvlJc w:val=\"left\"/>"
        "<w:pPr><w:ind w:left=\"720\" w:hanging=\"360\"/></w:pPr></w:lvl>"
        "<w:lvl w:ilvl=\"1\"><w:start w:val=\"1\"/><w:numFmt w:val=\"lowerLetter\"/>"
        "<w:lvlText w:val=\"%2)\"/><w:lvlJc w:val=\"left\"/>"
        "<w:pPr><w:ind w:left=\"1440\" w:hanging=\"360\"/></w:pPr></w:lvl>"
        "<w:lvl w:ilvl=\"2\"><w:start w:val=\"1\"/><w:numFmt w:val=\"lowerRoman\"/>"
        "<w:lvlText w:val=\"%3.\"/><w:lvlJc w:val=\"left\"/>"
        "<w:pPr><w:ind w:left=\"2160\" w:hanging=\"360\"/></w:pPr></w:lvl>"
        "</w:abstractNum>"
        // 无序列表定义
        "<w:abstractNum w:abstractNumId=\"1\">"
        "<w:multiLevelType w:val=\"hybridMultilevel\"/>"
        "<w:lvl w:ilvl=\"0\"><w:start w:val=\"1\"/><w:numFmt w:val=\"bullet\"/>"
        "<w:lvlText w:val=\"\xEF\x82\xB7\"/><w:lvlJc w:val=\"left\"/>"
        "<w:pPr><w:ind w:left=\"720\" w:hanging=\"360\"/></w:pPr></w:lvl>"
        "<w:lvl w:ilvl=\"1\"><w:start w:val=\"1\"/><w:numFmt w:val=\"bullet\"/>"
        "<w:lvlText w:val=\"o\"/><w:lvlJc w:val=\"left\"/>"
        "<w:pPr><w:ind w:left=\"1440\" w:hanging=\"360\"/></w:pPr></w:lvl>"
        "<w:lvl w:ilvl=\"2\"><w:start w:val=\"1\"/><w:numFmt w:val=\"bullet\"/>"
        "<w:lvlText w:val=\"\xEF\x82\xA7\"/><w:lvlJc w:val=\"left\"/>"
        "<w:pPr><w:ind w:left=\"2160\" w:hanging=\"360\"/></w:pPr></w:lvl>"
        "</w:abstractNum>"
        "<w:num w:numId=\"1\"><w:abstractNumId w:val=\"0\"/></w:num>"
        "<w:num w:numId=\"2\"><w:abstractNumId w:val=\"1\"/></w:num>"
        "</w:numbering>\n");
}

QString buildCoreXml(const QString &title)
{
    const QString now = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
    return QStringLiteral(
        "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
        "<cp:coreProperties "
        "xmlns:cp=\"http://schemas.openxmlformats.org/package/2006/metadata/core-properties\" "
        "xmlns:dc=\"http://purl.org/dc/elements/1.1/\" "
        "xmlns:dcterms=\"http://purl.org/dc/terms/\" "
        "xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\">"
        "<dc:title>%1</dc:title>"
        "<dc:creator>Markdown Editor</dc:creator>"
        "<cp:lastModifiedBy>Markdown Editor</cp:lastModifiedBy>"
        "<dcterms:created xsi:type=\"dcterms:W3CDTF\">%2</dcterms:created>"
        "<dcterms:modified xsi:type=\"dcterms:W3CDTF\">%2</dcterms:modified>"
        "</cp:coreProperties>\n").arg(xesc(title), now);
}

QString buildAppXml()
{
    return QStringLiteral(
        "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
        "<Properties xmlns=\"http://schemas.openxmlformats.org/officeDocument/2006/extended-properties\" "
        "xmlns:vt=\"http://schemas.openxmlformats.org/officeDocument/2006/docPropsVTypes\">"
        "<Application>Markdown Editor</Application>"
        "<AppVersion>1.0</AppVersion>"
        "</Properties>\n");
}

}  // namespace

// ===========================================================================
// docx
// ===========================================================================

QByteArray Exporter::toDocx(const QString &markdown)
{
    const QList<DocBlock> blocks = DocModel::parse(markdown);
    const QString title = DocModel::documentTitle(blocks);

    QStringList hyperlinks;
    const QString documentXml = buildDocumentXml(blocks, hyperlinks);

    ZipWriter zip;
    // 注意顺序：[Content_Types].xml 必须在最前（OPC 规范建议）
    zip.addFile(QStringLiteral("[Content_Types].xml"), buildContentTypesXml().toUtf8());
    zip.addFile(QStringLiteral("_rels/.rels"), buildRelsXml().toUtf8());
    zip.addFile(QStringLiteral("word/document.xml"), documentXml.toUtf8());
    zip.addFile(QStringLiteral("word/styles.xml"), buildStylesXml().toUtf8());
    zip.addFile(QStringLiteral("word/numbering.xml"), buildNumberingXml().toUtf8());
    zip.addFile(QStringLiteral("word/_rels/document.xml.rels"),
                buildDocumentRelsXml(hyperlinks).toUtf8());
    zip.addFile(QStringLiteral("docProps/core.xml"), buildCoreXml(title).toUtf8());
    zip.addFile(QStringLiteral("docProps/app.xml"), buildAppXml().toUtf8());

    return zip.toByteArray();
}

// ===========================================================================
// EPUB 3
// ===========================================================================

namespace {

QString epubCss()
{
    return QStringLiteral(
        "body { font-family: \"Microsoft YaHei\", serif; line-height: 1.7; "
        "margin: 0 1em; padding: 0; color: #222; }\n"
        "h1, h2, h3, h4, h5, h6 { font-weight: 600; line-height: 1.3; "
        "margin: 1.2em 0 0.6em; }\n"
        "h1 { font-size: 1.9em; border-bottom: 1px solid #ddd; padding-bottom: .3em; }\n"
        "h2 { font-size: 1.5em; border-bottom: 1px solid #eee; padding-bottom: .25em; }\n"
        "h3 { font-size: 1.25em; }\n"
        "p { margin: 0.7em 0; text-align: justify; }\n"
        "code { font-family: Consolas, monospace; background: #f4f4f4; "
        "padding: 0.1em 0.3em; border-radius: 3px; font-size: 0.92em; }\n"
        "pre { background: #f7f7f7; padding: 0.8em; overflow-x: auto; "
        "border-left: 3px solid #ddd; }\n"
        "pre code { background: none; padding: 0; }\n"
        "blockquote { margin: 0.8em 0; padding-left: 1em; "
        "border-left: 3px solid #ccc; color: #555; }\n"
        "table { border-collapse: collapse; width: 100%; margin: 1em 0; }\n"
        "th, td { border: 1px solid #ccc; padding: 0.4em 0.6em; text-align: left; }\n"
        "th { background: #f2f2f2; font-weight: 600; }\n"
        "mark { background: #fff3a3; }\n"
        "img { max-width: 100%; }\n"
        "hr { border: none; border-top: 1px solid #ccc; margin: 1.5em 0; }\n"
        ".task { font-family: monospace; }\n"
        "ul.tasks { list-style: none; padding-left: 1.2em; }\n"
        "a { color: #0366d6; text-decoration: none; }\n");
}

QString buildXhtmlPage(const QString &title, const QString &body)
{
    return QStringLiteral(
        "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
        "<!DOCTYPE html>\n"
        "<html xmlns=\"http://www.w3.org/1999/xhtml\" "
        "xmlns:epub=\"http://www.idpf.org/2007/ops\" xml:lang=\"zh-CN\">\n"
        "<head>\n"
        "<meta charset=\"utf-8\"/>\n"
        "<title>%1</title>\n"
        "<link rel=\"stylesheet\" type=\"text/css\" href=\"style.css\"/>\n"
        "</head>\n"
        "<body>\n%2</body>\n</html>\n").arg(xesc(title), body);
}

QString buildContainerXml()
{
    return QStringLiteral(
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        "<container version=\"1.0\" "
        "xmlns=\"urn:oasis:names:tc:opendocument:xmlns:container\">\n"
        "<rootfiles>\n"
        "<rootfile full-path=\"OEBPS/content.opf\" "
        "media-type=\"application/oebps-package+xml\"/>\n"
        "</rootfiles>\n"
        "</container>\n");
}

QString buildNavXhtml(const QString &title, const QList<DocBlock> &blocks)
{
    QString nav;
    nav += QStringLiteral("<nav epub:type=\"toc\" id=\"toc\">\n<h1>目录</h1>\n<ol>\n");
    // 只取前两级标题，避免过深
    for (int i = 0; i < blocks.size(); ++i) {
        const DocBlock &b = blocks.at(i);
        if (b.type == DocBlock::Heading && b.level <= 3) {
            nav += QStringLiteral("<li><a href=\"text.xhtml#h%1\">%2</a></li>\n")
                       .arg(i).arg(xesc(plainOf(b.inlines)));
        }
    }
    nav += QStringLiteral("</ol>\n</nav>\n");
    return buildXhtmlPage(title, nav);
}

QString buildContentOpf(const QString &title)
{
    const QString uuid = QUuid::createUuid().toString(QUuid::WithoutBraces);
    const QString modified = QDateTime::currentDateTimeUtc().toString(QStringLiteral("yyyy-MM-ddTHH:mm:ssZ"));
    return QStringLiteral(
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        "<package xmlns=\"http://www.idpf.org/2007/opf\" version=\"3.0\" "
        "unique-identifier=\"bookid\">\n"
        "<metadata xmlns:dc=\"http://purl.org/dc/elements/1.1/\">\n"
        "<dc:identifier id=\"bookid\">urn:uuid:%1</dc:identifier>\n"
        "<dc:title>%2</dc:title>\n"
        "<dc:language>zh-CN</dc:language>\n"
        "<dc:creator>Markdown Editor</dc:creator>\n"
        "<meta property=\"dcterms:modified\">%3</meta>\n"
        "</metadata>\n"
        "<manifest>\n"
        "<item id=\"nav\" href=\"nav.xhtml\" media-type=\"application/xhtml+xml\" properties=\"nav\"/>\n"
        "<item id=\"css\" href=\"style.css\" media-type=\"text/css\"/>\n"
        "<item id=\"text\" href=\"text.xhtml\" media-type=\"application/xhtml+xml\"/>\n"
        "</manifest>\n"
        "<spine>\n"
        "<itemref idref=\"text\"/>\n"
        "</spine>\n"
        "</package>\n").arg(uuid, xesc(title), modified);
}

}  // namespace

QByteArray Exporter::toEpub(const QString &markdown)
{
    const QList<DocBlock> blocks = DocModel::parse(markdown);
    const QString title = DocModel::documentTitle(blocks);
    const QString body = QString::fromUtf8(toXhtml(markdown));

    ZipWriter zip;
    // mimetype 必须是 ZIP 里的第一个条目，且不压缩（STORE）
    zip.addFile(QStringLiteral("mimetype"), QByteArrayLiteral("application/epub+zip"));
    zip.addFile(QStringLiteral("META-INF/container.xml"), buildContainerXml().toUtf8());
    zip.addFile(QStringLiteral("OEBPS/content.opf"), buildContentOpf(title).toUtf8());
    zip.addFile(QStringLiteral("OEBPS/nav.xhtml"), buildNavXhtml(title, blocks).toUtf8());
    zip.addFile(QStringLiteral("OEBPS/style.css"), epubCss().toUtf8());
    zip.addFile(QStringLiteral("OEBPS/text.xhtml"),
                buildXhtmlPage(title, body).toUtf8());

    return zip.toByteArray();
}

// ===========================================================================
// 纯文本类格式的分发
// ===========================================================================

bool Exporter::generateText(const QString &id, const QString &markdown,
                            const QString &html, QByteArray &out)
{
    if (id == QLatin1String("markdown")) {
        out = toMarkdown(markdown);
        return true;
    }
    if (id == QLatin1String("plaintext")) {
        out = toPlainText(markdown);
        return true;
    }
    if (id == QLatin1String("latex")) {
        out = toLatex(markdown);
        return true;
    }
    if (id == QLatin1String("rtf")) {
        out = toRtf(markdown);
        return true;
    }
    if (id == QLatin1String("opml")) {
        out = toOpml(markdown);
        return true;
    }
    if (id == QLatin1String("mediawiki")) {
        out = toMediaWiki(markdown);
        return true;
    }
    if (id == QLatin1String("rst")) {
        out = toRst(markdown);
        return true;
    }
    if (id == QLatin1String("textile")) {
        out = toTextile(markdown);
        return true;
    }
    if (id == QLatin1String("docx")) {
        out = toDocx(markdown);
        return true;
    }
    if (id == QLatin1String("epub")) {
        out = toEpub(markdown);
        return true;
    }
    if (id == QLatin1String("html")) {
        out = html.toUtf8();
        return true;
    }
    // pdf / odt / image 需要 QPrinter / QTextDocument 上下文，走 exportToFile
    return false;
}
