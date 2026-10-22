#include "docmodel.h"

#include <QRegularExpression>

// ===========================================================================
// 行内解析
// ===========================================================================

namespace {

/**
 * 解析行内序列。
 *
 * 用「扫描 + 递归」而不是正则全局替换：
 * 正则替换难以正确处理嵌套（***粗斜体***）和不配对标记。
 */
void parseInlineRun(const QString &text, int from, int to,
                    DocInline base, QList<DocInline> &out);

void emitInline(QList<DocInline> &out, const DocInline &inl)
{
    if (inl.text.isEmpty()) {
        return;
    }
    // 合并相邻同样式片段，减少后续生成的碎片
    if (!out.isEmpty()) {
        DocInline &last = out.last();
        if (last.bold == inl.bold && last.italic == inl.italic &&
            last.code == inl.code && last.strike == inl.strike &&
            last.highlight == inl.highlight && last.underline == inl.underline &&
            last.superscript == inl.superscript && last.subscript == inl.subscript &&
            last.math == inl.math && last.link == inl.link &&
            last.imageSrc == inl.imageSrc) {
            last.text += inl.text;
            return;
        }
    }
    out.append(inl);
}

// 找配对的结束标记（简单、不跨段）
int findClose(const QString &text, const QString &marker, int start, int limit)
{
    const int idx = text.indexOf(marker, start);
    if (idx < 0 || idx >= limit) {
        return -1;
    }
    return idx;
}

void parseInlineRun(const QString &text, int from, int to,
                    DocInline base, QList<DocInline> &out)
{
    if (from >= to) {
        return;
    }

    int i = from;
    int plainStart = from;

    auto flushPlain = [&](int end) {
        if (end > plainStart) {
            DocInline d = base;
            d.text = text.mid(plainStart, end - plainStart);
            emitInline(out, d);
        }
    };

    while (i < to) {
        const QChar c = text.at(i);

        // ---- 行内代码 `code` ----
        if (c == QLatin1Char('`')) {
            const int close = findClose(text, QStringLiteral("`"), i + 1, to);
            if (close > i) {
                flushPlain(i);
                DocInline d = base;
                d.text = text.mid(i + 1, close - i - 1);
                d.code = true;
                emitInline(out, d);
                i = close + 1;
                plainStart = i;
                continue;
            }
        }

        // ---- 图片 ![alt](src) ----
        if (c == QLatin1Char('!') && i + 1 < to && text.at(i + 1) == QLatin1Char('[')) {
            const int altEnd = findClose(text, QStringLiteral("]"), i + 2, to);
            if (altEnd > 0 && altEnd + 1 < to && text.at(altEnd + 1) == QLatin1Char('(')) {
                const int srcEnd = findClose(text, QStringLiteral(")"), altEnd + 2, to);
                if (srcEnd > 0) {
                    flushPlain(i);
                    DocInline d = base;
                    d.imageAlt = text.mid(i + 2, altEnd - i - 2);
                    d.imageSrc = text.mid(altEnd + 2, srcEnd - altEnd - 2).trimmed();
                    d.text = d.imageAlt;
                    emitInline(out, d);
                    i = srcEnd + 1;
                    plainStart = i;
                    continue;
                }
            }
        }

        // ---- 链接 [text](href) ----
        if (c == QLatin1Char('[')) {
            const int altEnd = findClose(text, QStringLiteral("]"), i + 1, to);
            if (altEnd > 0 && altEnd + 1 < to && text.at(altEnd + 1) == QLatin1Char('(')) {
                const int hrefEnd = findClose(text, QStringLiteral(")"), altEnd + 2, to);
                if (hrefEnd > 0) {
                    flushPlain(i);
                    DocInline d = base;
                    d.link = text.mid(altEnd + 2, hrefEnd - altEnd - 2).trimmed();
                    d.underline = true;
                    const QString inner = text.mid(i + 1, altEnd - i - 1);
                    // 链接文字内部可能有格式，递归处理
                    parseInlineRun(inner, 0, inner.length(), d, out);
                    i = hrefEnd + 1;
                    plainStart = i;
                    continue;
                }
            }
        }

        // ---- 高亮 ==text== ----
        if (c == QLatin1Char('=') && i + 1 < to && text.at(i + 1) == QLatin1Char('=')) {
            const int close = findClose(text, QStringLiteral("=="), i + 2, to);
            if (close > i + 1) {
                flushPlain(i);
                DocInline d = base;
                d.highlight = true;
                const QString inner = text.mid(i + 2, close - i - 2);
                parseInlineRun(inner, 0, inner.length(), d, out);
                i = close + 2;
                plainStart = i;
                continue;
            }
        }

        // ---- 删除线 ~~text~~ ----
        if (c == QLatin1Char('~') && i + 1 < to && text.at(i + 1) == QLatin1Char('~')) {
            const int close = findClose(text, QStringLiteral("~~"), i + 2, to);
            if (close > i + 1) {
                flushPlain(i);
                DocInline d = base;
                d.strike = true;
                const QString inner = text.mid(i + 2, close - i - 2);
                parseInlineRun(inner, 0, inner.length(), d, out);
                i = close + 2;
                plainStart = i;
                continue;
            }
        }

        // ---- 粗体 / 斜体（** 与 *，*** 视为粗斜体） ----
        if (c == QLatin1Char('*')) {
            // *** 粗斜体
            if (i + 2 < to && text.mid(i, 3) == QLatin1String("***")) {
                const int close = findClose(text, QStringLiteral("***"), i + 3, to);
                if (close > i + 2) {
                    flushPlain(i);
                    DocInline d = base;
                    d.bold = true;
                    d.italic = true;
                    const QString inner = text.mid(i + 3, close - i - 3);
                    parseInlineRun(inner, 0, inner.length(), d, out);
                    i = close + 3;
                    plainStart = i;
                    continue;
                }
            }
            // ** 粗体
            if (i + 1 < to && text.at(i + 1) == QLatin1Char('*')) {
                const int close = findClose(text, QStringLiteral("**"), i + 2, to);
                if (close > i + 1) {
                    flushPlain(i);
                    DocInline d = base;
                    d.bold = true;
                    const QString inner = text.mid(i + 2, close - i - 2);
                    parseInlineRun(inner, 0, inner.length(), d, out);
                    i = close + 2;
                    plainStart = i;
                    continue;
                }
            }
            // * 斜体
            const int close = findClose(text, QStringLiteral("*"), i + 1, to);
            if (close > i) {
                flushPlain(i);
                DocInline d = base;
                d.italic = true;
                const QString inner = text.mid(i + 1, close - i - 1);
                parseInlineRun(inner, 0, inner.length(), d, out);
                i = close + 1;
                plainStart = i;
                continue;
            }
        }

        // ---- 行内公式 $...$ ----
        if (c == QLatin1Char('$')) {
            const int close = findClose(text, QStringLiteral("$"), i + 1, to);
            if (close > i) {
                flushPlain(i);
                DocInline d = base;
                d.math = true;
                d.text = text.mid(i + 1, close - i - 1);
                emitInline(out, d);
                i = close + 1;
                plainStart = i;
                continue;
            }
        }

        // ---- 上标 ^text^ ----
        if (c == QLatin1Char('^')) {
            const int close = findClose(text, QStringLiteral("^"), i + 1, to);
            if (close > i) {
                flushPlain(i);
                DocInline d = base;
                d.superscript = true;
                d.text = text.mid(i + 1, close - i - 1);
                emitInline(out, d);
                i = close + 1;
                plainStart = i;
                continue;
            }
        }

        ++i;
    }

    flushPlain(to);
}

}  // namespace

QList<DocInline> DocModel::parseInlines(const QString &text)
{
    QList<DocInline> out;
    DocInline base;
    parseInlineRun(text, 0, text.length(), base, out);
    return out;
}

// ===========================================================================
// 块级解析
// ===========================================================================

namespace {

bool isFenceLine(const QString &t)
{
    return t.startsWith(QLatin1String("```")) || t.startsWith(QLatin1String("~~~"));
}

/**
 * 取围栏标记（``` 或 ~~~）。
 *
 * 关闭围栏必须用与开启时相同的标记：``` 开的块不能用 ~~~ 关。
 * 早期实现只用 isFenceLine 判断开关，``` 开的块会被 ~~~ 提前关掉。
 */
QString fenceMarkerOf(const QString &t)
{
    if (t.startsWith(QLatin1String("```"))) {
        return QStringLiteral("```");
    }
    if (t.startsWith(QLatin1String("~~~"))) {
        return QStringLiteral("~~~");
    }
    return QString();
}

// 把表格行切成单元格
QStringList splitTableRow(const QString &line)
{
    QString s = line.trimmed();
    if (s.startsWith(QLatin1Char('|'))) {
        s = s.mid(1);
    }
    if (s.endsWith(QLatin1Char('|'))) {
        s.chop(1);
    }
    QStringList cells = s.split(QLatin1Char('|'));
    for (QString &c : cells) {
        c = c.trimmed();
    }
    return cells;
}

bool isTableSeparator(const QString &line)
{
    // 分隔行：每格是可选的冒号 + 一个或多个横线 + 可选的冒号。
    //
    // 横线必须允许只有 1 个 —— GFM 规定「:-:」也是合法的居中对齐分隔符，
    // 早期写的是 -{2,}，于是 ":-:" 匹配不上，整张表被当成普通段落渲染。
    static QRegularExpression rx(
        QStringLiteral("^\\s*\\|?\\s*:?-+:?\\s*(\\|\\s*:?-+:?\\s*)*\\|?\\s*$"));
    return rx.match(line).hasMatch();
}

QString alignOf(const QString &cell)
{
    const QString t = cell.trimmed();
    const bool left = t.startsWith(QLatin1Char(':'));
    const bool right = t.endsWith(QLatin1Char(':'));
    if (left && right) {
        return QStringLiteral("center");
    }
    if (right) {
        return QStringLiteral("right");
    }
    if (left) {
        return QStringLiteral("left");
    }
    return QString();
}

}  // namespace

QList<DocBlock> DocModel::parse(const QString &markdown)
{
    QList<DocBlock> blocks;
    const QStringList lines = markdown.split(QLatin1Char('\n'));

    bool inCode = false;
    QString codeLang;
    QString codeFence;
    int codeStartLine = -1;
    QStringList codeLines;

    int i = 0;
    while (i < lines.size()) {
        const QString rawLine = lines.at(i);
        const QString t = rawLine.trimmed();

        // ---- 围栏代码块 ----
        if (!inCode && isFenceLine(t)) {
            inCode = true;
            codeFence = fenceMarkerOf(t);
            codeLang = t.mid(3).trimmed();
            codeStartLine = i;
            codeLines.clear();
            ++i;
            continue;
        }
        if (inCode) {
            // 关闭围栏必须用与开启时相同的标记
            if (!codeFence.isEmpty() && t.startsWith(codeFence)) {
                DocBlock b;
                b.type = DocBlock::CodeBlock;
                b.lang = codeLang;
                b.codeLines = codeLines;
                b.sourceLine = codeStartLine;
                blocks.append(b);
                inCode = false;
                codeLang.clear();
                codeFence.clear();
                codeLines.clear();
            } else {
                codeLines.append(rawLine);
            }
            ++i;
            continue;
        }

        // ---- 空行 ----
        if (t.isEmpty()) {
            ++i;
            continue;
        }

        // ---- 公式块 $$ ... $$ ----
        if (t == QLatin1String("$$")) {
            QStringList mathLines;
            ++i;
            while (i < lines.size() && lines.at(i).trimmed() != QLatin1String("$$")) {
                mathLines.append(lines.at(i));
                ++i;
            }
            if (i < lines.size()) {
                ++i;  // 跳过结束 $$
            }
            DocBlock b;
            b.type = DocBlock::MathBlock;
            b.codeLines = mathLines;
            blocks.append(b);
            continue;
        }

        // ---- 水平分割线 ----
        {
            static QRegularExpression hrRx(QStringLiteral("^(\\*\\s*){3,}$|^(-\\s*){3,}$|^(_\\s*){3,}$"));
            if (hrRx.match(t).hasMatch()) {
                DocBlock b;
                b.type = DocBlock::HorizontalRule;
                blocks.append(b);
                ++i;
                continue;
            }
        }

        // ---- 标题 ----
        {
            // 标题识别规则只有一份（parseHeadingLine），
            // 大纲面板、预览区、[TOC]、锚点跳转共用它。
            int level = 0;
            QString text;
            if (parseHeadingLine(t, level, text)) {
                DocBlock b;
                b.type = DocBlock::Heading;
                b.level = level;
                // 标题原文：锚点由它算出（raw -> anchorBase），
                // 行内渲染则基于解析后的 inlines。
                b.raw = text;
                b.inlines = parseInlines(text);
                b.sourceLine = i;
                blocks.append(b);
                ++i;
                continue;
            }
        }

        // ---- 表格 ----
        if (t.contains(QLatin1Char('|')) && i + 1 < lines.size() &&
            isTableSeparator(lines.at(i + 1))) {
            DocBlock b;
            b.type = DocBlock::Table;
            b.sourceLine = i;
            b.tableRows.append(splitTableRow(rawLine));

            // 分隔行 -> 对齐信息
            {
                const QStringList seps = splitTableRow(lines.at(i + 1));
                for (const QString &s : seps) {
                    b.tableAligns.append(alignOf(s));
                }
            }
            i += 2;
            while (i < lines.size() && lines.at(i).contains(QLatin1Char('|')) &&
                   !lines.at(i).trimmed().isEmpty()) {
                b.tableRows.append(splitTableRow(lines.at(i)));
                ++i;
            }
            blocks.append(b);
            continue;
        }

        // ---- 引用（可嵌套 > >） ----
        if (t.startsWith(QLatin1Char('>'))) {
            DocBlock b;
            b.type = DocBlock::Quote;
            b.sourceLine = i;
            int depth = 0;
            QString content = t;
            while (content.startsWith(QLatin1Char('>'))) {
                content = content.mid(1);
                if (content.startsWith(QLatin1Char(' '))) {
                    content = content.mid(1);
                }
                ++depth;
            }
            b.level = depth;
            // 收集连续的引用行
            QStringList quoteLines;
            quoteLines.append(content);
            ++i;
            while (i < lines.size() && lines.at(i).trimmed().startsWith(QLatin1Char('>'))) {
                QString l = lines.at(i).trimmed();
                while (l.startsWith(QLatin1Char('>'))) {
                    l = l.mid(1);
                    if (l.startsWith(QLatin1Char(' '))) {
                        l = l.mid(1);
                    }
                }
                quoteLines.append(l);
                ++i;
            }
            b.inlines = parseInlines(quoteLines.join(QLatin1Char(' ')));
            blocks.append(b);
            continue;
        }

        // ---- 列表（无序 / 有序 / 任务） ----
        {
            static QRegularExpression ulRx(QStringLiteral("^(\\s*)([-+*])\\s+(.*)$"));
            static QRegularExpression olRx(QStringLiteral("^(\\s*)(\\d+)[.)]\\s+(.*)$"));
            static QRegularExpression taskRx(QStringLiteral("^(\\s*)([-+*])\\s+\\[([ xX])\\]\\s+(.*)$"));

            const QRegularExpressionMatch tm = taskRx.match(rawLine);
            const QRegularExpressionMatch um = ulRx.match(rawLine);
            const QRegularExpressionMatch om = olRx.match(rawLine);

            if (tm.hasMatch() || um.hasMatch() || om.hasMatch()) {
                if (tm.hasMatch()) {
                    DocBlock b;
                    b.type = DocBlock::ListItem;
                    b.isTask = true;
                    b.checked = tm.captured(3).toLower() == QLatin1String("x");
                    b.level = static_cast<int>(tm.captured(1).length() / 2);
                    b.inlines = parseInlines(tm.captured(4));
                    b.sourceLine = i;
                    blocks.append(b);
                } else if (um.hasMatch()) {
                    DocBlock b;
                    b.type = DocBlock::ListItem;
                    b.ordered = false;
                    b.level = static_cast<int>(um.captured(1).length() / 2);
                    b.inlines = parseInlines(um.captured(3));
                    b.sourceLine = i;
                    blocks.append(b);
                } else {
                    DocBlock b;
                    b.type = DocBlock::ListItem;
                    b.ordered = true;
                    b.level = static_cast<int>(om.captured(1).length() / 2);
                    b.inlines = parseInlines(om.captured(3));
                    b.sourceLine = i;
                    blocks.append(b);
                }
                ++i;
                continue;
            }
        }

        // ---- [TOC] 目录块 ----
        // 对应 Typora 的 [TOC] 语法，渲染为可点击的目录
        if (t.compare(QLatin1String("[TOC]"), Qt::CaseInsensitive) == 0 ||
            t.compare(QLatin1String("[ toc ]"), Qt::CaseInsensitive) == 0) {
            DocBlock b;
            b.type = DocBlock::Toc;
            b.sourceLine = i;
            blocks.append(b);
            ++i;
            continue;
        }

        // ---- 普通段落（连续非空行合并） ----
        {
            QStringList paraLines;
            const int paraStart = i;
            paraLines.append(t);
            ++i;
            while (i < lines.size()) {
                const QString nt = lines.at(i).trimmed();
                if (nt.isEmpty() || isFenceLine(nt) || nt.startsWith(QLatin1Char('#')) ||
                    nt.startsWith(QLatin1Char('>')) || nt == QLatin1String("$$") ||
                    nt.startsWith(QLatin1String("- ")) || nt.startsWith(QLatin1String("* ")) ||
                    nt.startsWith(QLatin1String("+ "))) {
                    break;
                }
                // 有序列表也要断开：漏了这一条会让「段落」把紧随其后的
                // "1. 第一" 吞进段落里，列表就整个不见了。
                static const QRegularExpression orderedStart(
                    QStringLiteral("^\\d+[.)]\\s+"));
                if (orderedStart.match(nt).hasMatch()) {
                    break;
                }
                paraLines.append(nt);
                ++i;
            }
            DocBlock b;
            b.type = DocBlock::Paragraph;
            b.inlines = parseInlines(paraLines.join(QLatin1Char(' ')));
            b.sourceLine = paraStart;
            blocks.append(b);
            continue;
        }
    }

    // 未闭合的代码块也收进来，避免内容丢失
    if (inCode) {
        DocBlock b;
        b.type = DocBlock::CodeBlock;
        b.lang = codeLang;
        b.codeLines = codeLines;
        b.sourceLine = codeStartLine;
        blocks.append(b);
    }

    return blocks;
}

// ===========================================================================
// 标题与锚点
// ===========================================================================

/**
 * 去掉标题尾部的闭合 # 序列。
 *
 * CommonMark 规则：闭合序列要么独占整段，要么前面有空白。
 * 早期实现是「while (endsWith('#')) chop(1)」，
 * 会把 "## C#" 剥成 "C" —— C# 里的 # 是标题文本的一部分。
 */
QString DocModel::stripClosingHashes(const QString &text)
{
    static const QRegularExpression closingHashes(QStringLiteral("(?:^|\\s)#+$"));
    QString s = text;
    s.remove(closingHashes);
    return s.trimmed();
}

/**
 * 解析单行是否为 ATX 标题。
 */
bool DocModel::parseHeadingLine(const QString &trimmedLine, int &level, QString &text)
{
    // 注意：必须是 static const。
    // 在逐行循环里用局部变量的话，每行都会重新编译一次正则 ——
    // 5MB 文档 8.6 万行就是 17 万次编译，占掉大纲刷新 1 秒以上的时间。
    static const QRegularExpression headingRx(
        QStringLiteral("^(#{1,6})\\s+(.*)$"));
    const QRegularExpressionMatch m = headingRx.match(trimmedLine);
    if (!m.hasMatch()) {
        return false;
    }
    level = m.captured(1).length();
    text = stripClosingHashes(m.captured(2).trimmed());
    return !text.isEmpty();
}

/**
 * 轻量扫描 Markdown 中的 ATX 标题。
 *
 * 规则与 DocModel::parse 完全一致（同一份正则与同一个闭合 # 处理），
 * 因此大纲面板列出的标题、预览区渲染出的标题、[TOC] 里的条目
 * 必然是同一批 —— 早期这三处各写一遍，已经漂移过。
 */
void DocModel::collectHeadings(const QString &markdown,
                               QList<int> &lines,
                               QList<int> &levels,
                               QList<QString> &texts)
{
    lines.clear();
    levels.clear();
    texts.clear();

    const QStringList allLines = markdown.split(QLatin1Char('\n'));
    bool inFence = false;
    QString fenceMarker;

    for (int i = 0; i < allLines.size(); ++i) {
        const QString trimmed = allLines.at(i).trimmed();

        // 围栏代码块（``` 或 ~~~）内的内容不是标题。
        // 关闭围栏必须用与开启时相同的标记。
        const QString marker = fenceMarkerOf(trimmed);
        if (!marker.isEmpty()) {
            if (!inFence) {
                inFence = true;
                fenceMarker = marker;
            } else if (marker == fenceMarker) {
                inFence = false;
            }
            continue;
        }
        if (inFence) {
            continue;
        }

        int level = 0;
        QString text;
        if (!parseHeadingLine(trimmed, level, text)) {
            continue;
        }

        lines.append(i);
        levels.append(level);
        texts.append(text);
    }
}

/**
 * 标题锚点基名：转小写，非字母数字的连续片段压成一个连字符。
 *
 * 早期这里用的是正则 "[^\w\u4e00-\u9fa5]+"，
 * 但 PCRE2 不支持 \uXXXX 这种写法 —— 整个正则非法，
 * QString::replace() 于是什么都不做，
 * 结果锚点实际上只是「原文转小写」，连字符化从来没生效过。
 * （这个 bug 藏得很深：正则非法时 Qt 只发一条警告，不报错。）
 *
 * 现在逐字符判断。QChar::isLetterOrNumber() 对汉字同样成立
 * —— 汉字在 Unicode 里属于 Lo（Other Letter）类别。
 */
QString DocModel::anchorBase(const QString &text)
{
    QString s;
    s.reserve(text.size());

    bool pendingSeparator = false;
    for (const QChar c : text) {
        if (c.isLetterOrNumber()) {
            // 分隔符只在两侧都有内容时才输出，
            // 这样首尾的符号自然被去掉。
            if (pendingSeparator && !s.isEmpty()) {
                s += QLatin1Char('-');
            }
            pendingSeparator = false;
            s += c.toLower();
        } else {
            pendingSeparator = true;
        }
    }
    return s;
}

/**
 * 生成唯一锚点：同名标题依次追加 -1 / -2 ...
 */
QString DocModel::uniqueAnchor(const QString &base, QMap<QString, int> &used)
{
    // used[base] 记录已出现的次数。
    // 第一次出现返回 base；第二次返回 base-1；第三次 base-2 ...
    if (!used.contains(base)) {
        used[base] = 1;
        return base;
    }
    const int n = used[base];
    used[base] = n + 1;
    return QString("%1-%2").arg(base).arg(n);
}

QString DocModel::toPlainText(const QList<DocBlock> &blocks)
{
    QStringList out;
    for (const DocBlock &b : blocks) {
        switch (b.type) {
        case DocBlock::Heading:
        case DocBlock::Paragraph:
        case DocBlock::Quote: {
            QString line;
            for (const DocInline &inl : b.inlines) {
                line += inl.text;
            }
            out << line;
            break;
        }
        case DocBlock::ListItem: {
            QString prefix = b.isTask ? (b.checked ? QStringLiteral("- [x] ")
                                                   : QStringLiteral("- [ ] "))
                                      : (b.ordered ? QStringLiteral("1. ")
                                                   : QStringLiteral("- "));
            prefix = QString(b.level * 2, QLatin1Char(' ')) + prefix;
            QString line;
            for (const DocInline &inl : b.inlines) {
                line += inl.text;
            }
            out << prefix + line;
            break;
        }
        case DocBlock::CodeBlock:
        case DocBlock::MathBlock:
            out << b.codeLines.join(QLatin1Char('\n'));
            break;
        case DocBlock::Table:
            for (const QStringList &row : b.tableRows) {
                out << row.join(QStringLiteral(" | "));
            }
            break;
        case DocBlock::HorizontalRule:
            out << QStringLiteral("---");
            break;
        case DocBlock::RawHtml:
            out << b.raw;
            break;
        case DocBlock::Toc:
            // 目录块在纯文本里保留标记（可往返）
            out << QStringLiteral("[TOC]");
            break;
        }
    }
    return out.join(QLatin1Char('\n'));
}

QString DocModel::documentTitle(const QList<DocBlock> &blocks)
{
    for (const DocBlock &b : blocks) {
        if (b.type == DocBlock::Heading && b.level == 1 && !b.inlines.isEmpty()) {
            QString t;
            for (const DocInline &inl : b.inlines) {
                t += inl.text;
            }
            return t.trimmed();
        }
    }
    for (const DocBlock &b : blocks) {
        if (b.type == DocBlock::Paragraph && !b.inlines.isEmpty()) {
            QString t;
            for (const DocInline &inl : b.inlines) {
                t += inl.text;
            }
            if (!t.trimmed().isEmpty()) {
                return t.trimmed();
            }
        }
    }
    return QStringLiteral("Untitled");
}
