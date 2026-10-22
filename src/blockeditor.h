#ifndef BLOCKEDITOR_H
#define BLOCKEDITOR_H

#include <QString>
#include <QStringList>

/**
 * Markdown 块级操作引擎。
 *
 * 支撑 Typora「段落」菜单的全部操作：
 *   - 标题级别提升 / 降低
 *   - 段落 / 引用 / 列表 / 任务列表 的块类型切换
 *   - 表格行 / 表格列的插入删除
 *   - 上下插入段落
 *   - 脚注 / 水平分割线 / 内容目录 / YAML Front Matter 的插入
 *
 * 设计原则：所有操作都在「整段文本」上做纯函数变换，
 * 输入输出都是 QString，便于单元测试，也便于在多处复用。
 */
class BlockEditor
{
public:
    // 操作结果
    struct Result {
        bool changed = false;
        QString text;
        int selStart = 0;
        int selEnd = 0;
    };

    // 当前行（光标所在行）的类型
    enum LineType {
        Unknown, Heading, Paragraph, ListItem, TaskItem,
        Quote, CodeBlock, TableRow, MathBlock, HtmlBlock
    };

    // 判定某行类型
    static LineType lineType(const QString &line);

    // 取得该行的标题级别（1-6），非标题返回 0
    static int headingLevel(const QString &line);

    /**
     * 提升标题级别（# -> ##）。
     * 非标题行会先转成一级标题。
     */
    static Result increaseHeading(const QString &text, int cursorPos);

    /**
     * 降低标题级别（## -> #）。
     * 已是一级标题则取消标题（变回段落）。
     */
    static Result decreaseHeading(const QString &text, int cursorPos);

    /**
     * 切换当前行为指定块类型。
     * type 取值：paragraph / quote / ul / ol / task / code / math / table
     */
    static Result toggleBlock(const QString &text, int cursorPos, const QString &type);

    /**
     * 在表格中插入行。
     * where: "above" | "below"
     */
    static Result insertTableRow(const QString &text, int cursorPos, const QString &where);

    /**
     * 在表格中删除当前行。
     */
    static Result deleteTableRow(const QString &text, int cursorPos);

    /**
     * 在表格中插入列。
     * where: "before" | "after"
     */
    static Result insertTableColumn(const QString &text, int cursorPos, const QString &where);

    /**
     * 在表格中删除当前列。
     */
    static Result deleteTableColumn(const QString &text, int cursorPos);

    /**
     * 在光标所在行上方/下方插入一个空段落。
     */
    static Result insertParagraph(const QString &text, int cursorPos, const QString &where);

    /**
     * 在光标所在行下方插入指定文本块（脚注、分割线、目录等）。
     */
    static Result insertBlock(const QString &text, int cursorPos, const QString &block);

    /**
     * 列出文档中的所有脚注定义（[^id]: ...），用于「创建脚注」。
     */
    static QStringList listFootnotes(const QString &text);

    /**
     * 生成一个新的脚注 id（形如 fn1 / fn2，避开已存在的）。
     */
    static QString newFootnoteId(const QString &text);
};

#endif // BLOCKEDITOR_H
