#ifndef DOCMODEL_H
#define DOCMODEL_H

#include <QList>
#include <QMap>
#include <QString>
#include <QStringList>

/**
 * Markdown 文档的中间表示（IR）。
 *
 * 设计目的：把「解析 Markdown」与「生成目标格式」解耦。
 * 解析只做一次，然后预览区与 docx / EPUB / RTF / LaTeX / XHTML 等
 * 各格式生成器都从同一棵 IR 树出发，行为一致、便于单测。
 *
 * 重要：预览区（HtmlRenderer）也必须走这里。
 * 早期预览区有一套独立的正则状态机（MainWindow::generateHtml），
 * 与本解析器对同一份文档给出不同结果 —— 表格多出空列、
 * 行内代码里的 ** 变成真粗体、~~~ 代码块失效等等。
 * 一份文档只能有一套解析规则。
 */

// 行内片段
struct DocInline {
    QString text;
    bool bold = false;
    bool italic = false;
    bool code = false;
    bool strike = false;
    bool highlight = false;
    bool underline = false;
    bool superscript = false;
    bool subscript = false;
    bool math = false;      // 行内公式 $...$
    QString link;           // 非空表示这是链接，值为 href
    QString imageSrc;       // 非空表示这是图片
    QString imageAlt;
};

// 块级元素
struct DocBlock {
    enum Type {
        Heading,        // level: 1-6
        Paragraph,
        ListItem,       // ordered + level + checked
        Quote,          // level 表示嵌套层数
        CodeBlock,      // lang + codeLines
        Table,          // tableRows（第一行是表头）
        HorizontalRule,
        MathBlock,      // codeLines 里是公式内容
        RawHtml,
        Toc             // [TOC] 目录块
    };

    Type type = Paragraph;
    int level = 0;
    bool ordered = false;
    bool checked = false;       // 任务项是否勾选
    bool isTask = false;
    QString lang;               // 代码块语言
    QString raw;                // 原始文本（Heading 存标题原文，RawHtml 存 HTML 原文）
    int sourceLine = -1;        // 该块在源码中的起始行号（0 基），-1 表示未知
    QList<DocInline> inlines;
    QStringList codeLines;
    QList<QStringList> tableRows;
    // 表格对齐（l/c/r），从分隔行解析
    QStringList tableAligns;
};

/**
 * Markdown 文档模型。
 */
class DocModel
{
public:
    /**
     * 解析 Markdown 为块级列表。
     *
     * 覆盖的语法：
     *   标题 #~######、段落、无序/有序列表（含嵌套）、任务列表、
     *   引用（含嵌套）、围栏代码块、表格、水平分割线、公式块、
     *   行内：粗体/斜体/删除线/行内代码/高亮/上下标/链接/图片/行内公式
     */
    static QList<DocBlock> parse(const QString &markdown);

    /**
     * 解析一段文本中的行内格式。
     * 公开以便单测。
     */
    static QList<DocInline> parseInlines(const QString &text);

    /**
     * 纯文本化：去掉所有行内标记（供纯文本导出与标题锚点用）。
     */
    static QString toPlainText(const QList<DocBlock> &blocks);

    /**
     * 取文档标题（第一个一级标题，没有则取第一个非空段落）。
     */
    static QString documentTitle(const QList<DocBlock> &blocks);

    // -----------------------------------------------------------------------
    // 标题与锚点
    //
    // 标题的「识别规则」和「锚点生成规则」各只有一份实现，
    // 大纲面板、预览区、[TOC]、锚点跳转都用它们 ——
    // 早期这三处各写了一遍，规则靠注释「保持一致」维系，
    // 实际已经漂移（大纲扫描未 trim 的行，预览扫描 trim 后的行）。
    // -----------------------------------------------------------------------

    /**
     * 轻量扫描 Markdown 中的 ATX 标题。
     *
     * 只做标题识别，不做行内解析 —— 大纲刷新在 5MB 文档上是 200ms 级的操作，
     * 不能为此跑一遍完整解析。
     *
     * @param markdown 源文本
     * @param lines    每个标题的源码行号（0 基）
     * @param levels   每个标题的级别（1~6）
     * @param texts    每个标题的原文（未去掉行内标记）
     */
    static void collectHeadings(const QString &markdown,
                                QList<int> &lines,
                                QList<int> &levels,
                                QList<QString> &texts);

    /**
     * 解析单行是否为 ATX 标题。
     *
     * @param trimmedLine 已经 trim 过的行
     * @param level       输出：标题级别（1~6）
     * @param text        输出：标题原文（已去掉尾部的闭合 # 序列）
     * @return 是否是标题
     */
    static bool parseHeadingLine(const QString &trimmedLine, int &level, QString &text);

    /**
     * 去掉标题尾部的闭合 # 序列。
     *
     * CommonMark 规则：闭合序列要么独占整段，要么前面有空白。
     * 不能简单地「while (endsWith('#')) chop(1)」——
     * 那会把 "## C#" 剥成 "C"。
     */
    static QString stripClosingHashes(const QString &text);

    /**
     * 标题锚点基名：转小写，非单词字符连字符化，去掉首尾连字符。
     * 与 Typora 的 md-toc 锚点生成规则保持一致。
     */
    static QString anchorBase(const QString &text);

    /**
     * 生成唯一锚点：同名标题依次追加 -1 / -2 ...
     *
     * @param base 锚点基名
     * @param used 计数器表（同一个 base 出现第几次）
     */
    static QString uniqueAnchor(const QString &base, QMap<QString, int> &used);
};

#endif // DOCMODEL_H
