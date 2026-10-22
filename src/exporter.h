#ifndef EXPORTER_H
#define EXPORTER_H

#include <QList>
#include <QString>
#include <QStringList>

#include "docmodel.h"

/**
 * 多格式导出器。
 *
 * 与 Typora「文件 > 导出」子菜单对齐，支持：
 *   HTML / PDF / Word(docx) / OpenOffice(odt) / EPUB / LaTeX / RTF /
 *   OPML / Wiki(MediaWiki) / reStructuredText / Textile / Image / Markdown / Plain Text
 *
 * 其中：
 *   - HTML / PDF / Markdown / 纯文本 / ODT：交给 Qt 原生能力
 *   - docx / EPUB：用 ZipWriter 构造 OOXML / EPUB 容器
 *   - LaTeX / RTF / OPML / Wiki / RST / Textile：从 IR 直接生成文本
 *   - Image：把 QTextDocument 渲染到 QImage
 */
class Exporter
{
public:
    // 导出格式描述
    struct Format {
        QString id;           // 内部标识，如 "docx"
        QString displayName;  // 菜单显示名，如 "Word (.docx)"
        QString filter;       // 文件对话框过滤器
        QString defaultExt;   // 默认扩展名
    };

    // 全部支持的格式（顺序即菜单顺序，与 Typora 一致）
    static QList<Format> formats();

    // 按 id 取格式，找不到返回空
    static Format formatById(const QString &id);

    /**
     * 生成纯文本类内容（不需要 QWidget 上下文）。
     *
     * @param id       格式 id
     * @param markdown 源 Markdown
     * @param html     generateHtml 产出的完整 HTML（HTML/ODT/Image 用）
     * @param out      输出内容
     * @return 成功与否；二进制格式（docx/epub/image）返回 false，需走 exportToFile
     */
    static bool generateText(const QString &id, const QString &markdown,
                             const QString &html, QByteArray &out);

    // ---- 各格式生成器（公开以便单测） ----
    static QByteArray toMarkdown(const QString &markdown);
    static QByteArray toPlainText(const QString &markdown);
    static QByteArray toLatex(const QString &markdown);
    static QByteArray toRtf(const QString &markdown);
    static QByteArray toOpml(const QString &markdown);
    static QByteArray toMediaWiki(const QString &markdown);
    static QByteArray toRst(const QString &markdown);
    static QByteArray toTextile(const QString &markdown);
    static QByteArray toDocx(const QString &markdown);
    static QByteArray toEpub(const QString &markdown);
    static QByteArray toXhtml(const QString &markdown);
};

#endif // EXPORTER_H
