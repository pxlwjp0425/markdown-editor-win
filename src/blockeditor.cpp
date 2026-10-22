#include "blockeditor.h"

#include <QRegularExpression>
#include <QSet>

// ===========================================================================
// 类型判定
// ===========================================================================

BlockEditor::LineType BlockEditor::lineType(const QString &line)
{
    const QString t = line.trimmed();
    if (t.isEmpty()) {
        return Unknown;
    }
    // 代码围栏
    if (t.startsWith("```") || t.startsWith("~~~")) {
        return CodeBlock;
    }
    // 标题
    if (QRegularExpression("^#{1,6}\\s").match(t).hasMatch()) {
        return Heading;
    }
    // 数学块
    if (t == "$$") {
        return MathBlock;
    }
    // 任务列表
    if (QRegularExpression("^([-+*]|\\d+[.)])\\s+\\[[ xX]\\]").match(t).hasMatch()) {
        return TaskItem;
    }
    // 普通列表
    if (QRegularExpression("^([-+*]|\\d+[.)])\\s+").match(t).hasMatch()) {
        return ListItem;
    }
    // 引用
    if (t.startsWith(">")) {
        return Quote;
    }
    // 表格行
    if (t.contains("|") && t.contains("---")) {
        return TableRow;
    }
    // HTML 块
    if (t.startsWith("<")) {
        return HtmlBlock;
    }
    return Paragraph;
}

int BlockEditor::headingLevel(const QString &line)
{
    static QRegularExpression rx("^(#{1,6})\\s+(.*)$");
    const QRegularExpressionMatch m = rx.match(line);
    return m.hasMatch() ? m.captured(1).length() : 0;
}

// ===========================================================================
// 通用辅助
// ===========================================================================

namespace {

// 把光标位置换算成行号与行内偏移
void locate(const QString &text, int cursorPos,
            int &lineStart, int &lineEnd, int &lineNo)
{
    lineStart = 0;
    lineNo = 0;
    int pos = 0;
    const QStringList lines = text.split('\n');
    for (int i = 0; i < lines.size(); ++i) {
        const int len = lines.at(i).length();
        if (cursorPos <= pos + len) {
            lineStart = pos;
            lineEnd = pos + len;
            return;
        }
        pos += len + 1;
        ++lineNo;
    }
    // 落在末尾
    lineStart = lineEnd = text.length();
}

QString replaceRange(const QString &text, int start, int len, const QString &repl)
{
    return text.left(start) + repl + text.mid(start + len);
}

// 去掉行首的块标记，返回标记所占的字符数
int leadingMarkerLen(const QString &line)
{
    int i = 0;
    while (i < line.length() && (line.at(i) == ' ' || line.at(i) == '\t')) {
        ++i;
    }
    if (i < line.length() && (line.at(i) == '>' || line.at(i) == '#')) {
        // 引用 / 标题标记：可以连续多个
        int j = i;
        while (j < line.length() && (line.at(j) == '>' || line.at(j) == '#')) {
            ++j;
        }
        while (j < line.length() && (line.at(j) == ' ' || line.at(j) == '\t')) {
            ++j;
        }
        return j;
    }
    if (i < line.length() && (line.at(i) == '-' || line.at(i) == '*' || line.at(i) == '+')) {
        // 列表标记：单个，且后面必须跟空白。
        //
        // 早期这里的 while 循环只认 '>' 与 '#'，列表标记 '-' / '*' / '+'
        // 根本没被吃掉 —— 于是把 "- 项目" 转成段落时得到的是 "- 项目"，
        // 列表标记原样留着；转成引用则变成 "> - 项目"。
        //
        // 必须是「单个 + 后跟空白」：否则 "***强调***" 会被当成列表标记，
        // "---" 也会被当成列表标记。
        int j = i + 1;
        while (j < line.length() && (line.at(j) == ' ' || line.at(j) == '\t')) {
            ++j;
        }
        if (j > i + 1) {
            return j;
        }
    }
    if (QRegularExpression("^\\d+[.)]\\s").match(line).hasMatch()) {
        return QRegularExpression("^\\s*\\d+[.)]\\s").match(line).capturedLength();
    }
    return 0;
}

}  // namespace

// ===========================================================================
// 标题级别
// ===========================================================================

BlockEditor::Result BlockEditor::increaseHeading(const QString &text, int cursorPos)
{
    Result r;
    int ls, le, ln;
    locate(text, cursorPos, ls, le, ln);
    const QString line = text.mid(ls, le - ls);

    int level = headingLevel(line);
    QString newLine;
    if (level > 0) {
        if (level >= 6) {
            return r;  // 已是六级，不再提升
        }
        newLine = QString(level + 1, '#') + line.mid(level);
    } else {
        // 非标题行：先剥离现有块标记，再转成一级标题
        const int mlen = leadingMarkerLen(line);
        const QString body = line.mid(mlen);
        newLine = QStringLiteral("# ") + body;
    }

    r.text = replaceRange(text, ls, le - ls, newLine);
    r.selStart = ls;
    r.selEnd = ls + newLine.length();
    r.changed = true;
    return r;
}

BlockEditor::Result BlockEditor::decreaseHeading(const QString &text, int cursorPos)
{
    Result r;
    int ls, le, ln;
    locate(text, cursorPos, ls, le, ln);
    const QString line = text.mid(ls, le - ls);

    const int level = headingLevel(line);
    QString newLine;
    if (level > 1) {
        newLine = QString(level - 1, '#') + line.mid(level);
    } else if (level == 1) {
        // 一级标题 -> 普通段落
        newLine = line.mid(1).trimmed();
    } else {
        return r;  // 不是标题
    }

    r.text = replaceRange(text, ls, le - ls, newLine);
    r.selStart = ls;
    r.selEnd = ls + newLine.length();
    r.changed = true;
    return r;
}

// ===========================================================================
// 块类型切换
// ===========================================================================

BlockEditor::Result BlockEditor::toggleBlock(const QString &text, int cursorPos, const QString &type)
{
    Result r;
    int ls, le, ln;
    locate(text, cursorPos, ls, le, ln);
    const QString line = text.mid(ls, le - ls);
    const QString t = line.trimmed();

    // 先剥掉已有的块标记，得到「纯内容」
    QString content = line;
    int markerLen = 0;
    QString curType;

    const int hl = headingLevel(line);
    if (hl > 0) {
        curType = QStringLiteral("heading");
        markerLen = hl;
        content = line.mid(hl).trimmed();
    } else if (t.startsWith(">")) {
        curType = QStringLiteral("quote");
        markerLen = leadingMarkerLen(line);
        content = line.mid(markerLen);
    } else if (QRegularExpression("^([-+*]|\\d+[.)])\\s+\\[[ xX]\\]").match(line).hasMatch()) {
        curType = QStringLiteral("task");
        markerLen = QRegularExpression("^(\\s*)([-*+]|\\d+[.)])\\s+\\[[ xX]\\]\\s*")
                        .match(line).capturedLength();
        content = line.mid(markerLen);
    } else if (QRegularExpression("^([-+*]|\\d+[.)])\\s+").match(line).hasMatch()) {
        curType = t.startsWith("1.") || QRegularExpression("^\\d+[.)]").match(t).hasMatch()
                      ? QStringLiteral("ol") : QStringLiteral("ul");
        markerLen = leadingMarkerLen(line);
        content = line.mid(markerLen);
    } else if (t.startsWith("```") || t.startsWith("~~~")) {
        curType = QStringLiteral("code");
        markerLen = leadingMarkerLen(line);
        content = line.mid(markerLen);
    } else if (t == "$$") {
        curType = QStringLiteral("math");
        markerLen = 0;
        content = line;
    } else {
        curType = QStringLiteral("paragraph");
        content = line;
    }

    QString newLine;
    if (type == QLatin1String("paragraph")) {
        newLine = content;
    } else if (type == QLatin1String("quote")) {
        newLine = (curType == QLatin1String("quote"))
                      ? content
                      : QStringLiteral("> ") + content;
    } else if (type == QLatin1String("ul")) {
        newLine = (curType == QLatin1String("ul")) ? content
                                                  : QStringLiteral("- ") + content;
    } else if (type == QLatin1String("ol")) {
        newLine = (curType == QLatin1String("ol")) ? content
                                                  : QStringLiteral("1. ") + content;
    } else if (type == QLatin1String("task")) {
        if (curType == QLatin1String("task")) {
            // 已经是任务项：保留任务类型，只切换勾选状态（[ ] <-> [x]）。
            // 注意不能直接用 content —— 那会把任务标记整段丢掉。
            static QRegularExpression taskMarkRx("^(\\s*)([-*+]|\\d+[.)]\\s+)\\[([ xX])\\]\\s*");
            const QRegularExpressionMatch tm = taskMarkRx.match(line);
            if (tm.hasMatch()) {
                const bool wasChecked = tm.captured(3).toLower() == "x";
                newLine = tm.captured(1) + tm.captured(2) + QLatin1Char('[')
                        + (wasChecked ? QLatin1Char(' ') : QLatin1Char('x'))
                        + QLatin1String("] ") + content;
            } else {
                newLine = QStringLiteral("- [ ] ") + content;
            }
        } else {
            newLine = QStringLiteral("- [ ] ") + content;
        }
    } else if (type == QLatin1String("code")) {
        newLine = (curType == QLatin1String("code")) ? content
                                                    : QStringLiteral("```\n") + content + QStringLiteral("\n```");
    } else if (type == QLatin1String("math")) {
        newLine = (curType == QLatin1String("math")) ? content
                                                    : QStringLiteral("$$\n") + content + QStringLiteral("\n$$");
    } else if (type == QLatin1String("heading1")) {
        newLine = QStringLiteral("# ") + content;
    } else {
        return r;
    }

    r.text = replaceRange(text, ls, le - ls, newLine);
    r.selStart = ls;
    r.selEnd = ls + newLine.length();
    r.changed = true;
    return r;
}

// ===========================================================================
// 表格操作
// ===========================================================================

namespace {

// 定位光标所在的表格，返回表格行的范围与列索引
bool locateTable(const QString &text, int cursorPos,
                 int &tblStart, int &tblEnd, int &rowIndex, int &colIndex)
{
    const QStringList lines = text.split('\n');
    int pos = 0;
    int start = -1, end = -1;
    for (int i = 0; i < lines.size(); ++i) {
        const QString l = lines.at(i);
        const bool isRow = l.contains('|');
        if (isRow) {
            if (start < 0) {
                start = pos;
            }
            end = pos + l.length();
        } else if (start >= 0 && l.trimmed().isEmpty()) {
            // 连续空行后仍有 | 才是同一张表；这里保守断开
            break;
        } else if (start >= 0) {
            break;
        }
        pos += l.length() + 1;
    }
    if (start < 0) {
        return false;
    }

    // 确定行与列
    const QString table = text.mid(start, end - start);
    const QStringList rows = table.split('\n');
    pos = start;
    rowIndex = -1;
    colIndex = -1;
    for (int i = 0; i < rows.size(); ++i) {
        const QString row = rows.at(i);
        if (cursorPos >= pos && cursorPos <= pos + row.length()) {
            rowIndex = i;
            // 找光标所在的列
            const int rel = cursorPos - pos;
            int col = 0;
            bool seen = false;
            for (int j = 0; j < row.length(); ++j) {
                if (row.at(j) == '|') {
                    if (j <= rel && !seen) {
                        col = j;
                        seen = true;
                    }
                }
            }
            colIndex = col;
            break;
        }
        pos += row.length() + 1;
    }
    if (rowIndex < 0) {
        rowIndex = 0;
    }
    tblStart = start;
    tblEnd = end;
    return true;
}

QStringList splitRow(const QString &row)
{
    QString s = row.trimmed();
    if (s.startsWith('|')) s = s.mid(1);
    if (s.endsWith('|')) s.chop(1);
    QStringList cells = s.split('|');
    for (QString &c : cells) {
        c = c.trimmed();
    }
    return cells;
}

QString joinRow(const QStringList &cells)
{
    QString s = QStringLiteral("|");
    for (const QString &c : cells) {
        s += QStringLiteral(" ") + c + QStringLiteral(" |");
    }
    return s;
}

}  // namespace

BlockEditor::Result BlockEditor::insertTableRow(const QString &text, int cursorPos, const QString &where)
{
    Result r;
    int ts, te, ri, ci;
    if (!locateTable(text, cursorPos, ts, te, ri, ci)) {
        return r;
    }
    const QStringList lines = text.mid(ts, te - ts).split('\n');
    if (ri < 0 || ri >= lines.size()) {
        return r;
    }

    // 新行的内容参照表头（或第二行）
    QStringList cells = splitRow(lines.at(ri));
    if (ri == 0 && lines.size() > 1) {
        cells = splitRow(lines.at(0));
    }
    for (QString &c : cells) {
        c.clear();
    }
    const QString newRow = joinRow(cells);

    QStringList out = lines;
    const int insertAt = (where == QLatin1String("above")) ? ri : ri + 1;
    out.insert(insertAt, newRow);

    const QString newTable = out.join('\n');
    r.text = replaceRange(text, ts, te - ts, newTable);
    r.selStart = ts;
    r.selEnd = ts + newTable.length();
    r.changed = true;
    return r;
}

BlockEditor::Result BlockEditor::deleteTableRow(const QString &text, int cursorPos)
{
    Result r;
    int ts, te, ri, ci;
    if (!locateTable(text, cursorPos, ts, te, ri, ci)) {
        return r;
    }
    QStringList lines = text.mid(ts, te - ts).split('\n');
    if (lines.size() <= 2) {
        return r;  // 只剩表头+分隔行，不能再删
    }
    if (ri <= 1) {
        return r;  // 表头与分隔行不能删
    }
    lines.removeAt(ri);
    const QString newTable = lines.join('\n');
    r.text = replaceRange(text, ts, te - ts, newTable);
    r.selStart = ts;
    r.selEnd = ts + newTable.length();
    r.changed = true;
    return r;
}

BlockEditor::Result BlockEditor::insertTableColumn(const QString &text, int cursorPos, const QString &where)
{
    Result r;
    int ts, te, ri, ci;
    if (!locateTable(text, cursorPos, ts, te, ri, ci)) {
        return r;
    }
    QStringList lines = text.mid(ts, te - ts).split('\n');
    if (lines.isEmpty()) {
        return r;
    }

    const int colCount = splitRow(lines.at(0)).size();
    // 目标列位置
    int target = ci;   // ci 是该行中 | 的字符位置，换算成列序号
    // 用表头行的分隔符位置推算
    {
        const QString head = lines.at(0);
        int idx = 0;
        int bar = 0;
        for (int j = 0; j < head.length(); ++j) {
            if (head.at(j) == '|') {
                if (bar == target) break;
                ++bar;
            }
            ++idx;
        }
        target = bar;
    }
    if (target < 0) target = 0;
    if (target > colCount) target = colCount;

    QStringList out;
    for (int i = 0; i < lines.size(); ++i) {
        QStringList cells = splitRow(lines.at(i));
        const int at = (where == QLatin1String("before")) ? target : target + 1;
        if (i == 1) {
            // 分隔行：新列填 ---
            cells.insert(qMin(at, cells.size()), QStringLiteral("---"));
        } else {
            cells.insert(qMin(at, cells.size()), QString());
        }
        out << joinRow(cells);
    }

    const QString newTable = out.join('\n');
    r.text = replaceRange(text, ts, te - ts, newTable);
    r.selStart = ts;
    r.selEnd = ts + newTable.length();
    r.changed = true;
    return r;
}

BlockEditor::Result BlockEditor::deleteTableColumn(const QString &text, int cursorPos)
{
    Result r;
    int ts, te, ri, ci;
    if (!locateTable(text, cursorPos, ts, te, ri, ci)) {
        return r;
    }
    QStringList lines = text.mid(ts, te - ts).split('\n');
    if (lines.isEmpty()) {
        return r;
    }
    const int colCount = splitRow(lines.at(0)).size();
    if (colCount <= 1) {
        return r;
    }

    int target = 0;
    {
        const QString head = lines.at(0);
        int bar = 0;
        for (int j = 0; j < head.length(); ++j) {
            if (head.at(j) == '|') {
                if (bar == ci) break;
                ++bar;
            }
        }
        target = bar;
    }
    if (target <= 0 || target > colCount) {
        return r;
    }

    QStringList out;
    for (const QString &l : lines) {
        QStringList cells = splitRow(l);
        if (target - 1 < cells.size()) {
            cells.removeAt(target - 1);
        }
        out << joinRow(cells);
    }
    const QString newTable = out.join('\n');
    r.text = replaceRange(text, ts, te - ts, newTable);
    r.selStart = ts;
    r.selEnd = ts + newTable.length();
    r.changed = true;
    return r;
}

// ===========================================================================
// 段落与块插入
// ===========================================================================

BlockEditor::Result BlockEditor::insertParagraph(const QString &text, int cursorPos, const QString &where)
{
    Result r;
    int ls, le, ln;
    locate(text, cursorPos, ls, le, ln);

    if (where == QLatin1String("above")) {
        r.text = text.left(ls) + QStringLiteral("\n") + text.mid(ls);
        r.selStart = ls;
        r.selEnd = ls;
    } else {
        r.text = text.left(le) + QStringLiteral("\n") + text.mid(le);
        r.selStart = le + 1;
        r.selEnd = le + 1;
    }
    r.changed = true;
    return r;
}

BlockEditor::Result BlockEditor::insertBlock(const QString &text, int cursorPos, const QString &block)
{
    Result r;
    int ls, le, ln;
    locate(text, cursorPos, ls, le, ln);

    QString insert;
    if (block == QLatin1String("footnote")) {
        const QString id = newFootnoteId(text);
        // 正文插引用，文档末尾插定义
        r.text = text.left(ls) + QStringLiteral("[^%1]").arg(id) + text.mid(ls);
        r.text = r.text.trimmed() + QStringLiteral("\n\n[^%1]: 脚注内容\n").arg(id);
        r.selStart = ls + 3;
        r.selEnd = ls + 3 + id.length();
        r.changed = true;
        return r;
    }
    if (block == QLatin1String("hr")) {
        insert = QStringLiteral("\n---\n");
    } else if (block == QLatin1String("toc")) {
        insert = QStringLiteral("\n[TOC]\n");
    } else if (block == QLatin1String("yaml")) {
        insert = QStringLiteral("\n---\ntitle: \n---\n");
    } else if (block == QLatin1String("math")) {
        insert = QStringLiteral("\n$$\n\n$$\n");
    } else {
        return r;
    }

    r.text = text.left(le) + insert + text.mid(le);
    r.selStart = le + insert.length();
    r.selEnd = r.selStart;
    r.changed = true;
    return r;
}

QStringList BlockEditor::listFootnotes(const QString &text)
{
    QStringList ids;
    static QRegularExpression rx("^\\[\\^([^\\]]+)\\]:");
    for (const QString &l : text.split('\n')) {
        const QRegularExpressionMatch m = rx.match(l);
        if (m.hasMatch()) {
            ids << m.captured(1);
        }
    }
    return ids;
}

QString BlockEditor::newFootnoteId(const QString &text)
{
    const QStringList used = listFootnotes(text);
    QSet<QString> set(used.begin(), used.end());
    for (int i = 1; i < 1000; ++i) {
        const QString id = QStringLiteral("fn%1").arg(i);
        if (!set.contains(id)) {
            return id;
        }
    }
    return QStringLiteral("fn");
}
