#include "inlineformatter.h"

#include <QObject>

InlineFormatter::InlineFormatter()
{
    // 与 Typora「格式」菜单一一对应
    registerFormat("bold",         {QStringLiteral("**"),   QStringLiteral("**"),   true});
    registerFormat("italic",       {QStringLiteral("*"),    QStringLiteral("*"),    true});
    registerFormat("strikethrough",{QStringLiteral("~~"),   QStringLiteral("~~"),   true});
    registerFormat("code",         {QStringLiteral("`"),    QStringLiteral("`"),    true});
    registerFormat("highlight",    {QStringLiteral("=="),   QStringLiteral("=="),   true});
    registerFormat("superscript",  {QStringLiteral("^"),    QStringLiteral("^"),    true});
    registerFormat("subscript",    {QStringLiteral("~"),    QStringLiteral("~"),    true});
    registerFormat("comment",      {QStringLiteral("<!--"), QStringLiteral("-->"),  false});
    registerFormat("math",         {QStringLiteral("$"),    QStringLiteral("$"),    true});
}

void InlineFormatter::registerFormat(const QString &id, const Spec &spec)
{
    m_specs.insert(id, spec);
    if (!m_order.contains(id)) {
        m_order.append(id);
    }
}

/**
 * 查找选区外层的包裹标记。
 *
 * 规则：从选区起点往前找 open 标记（要求紧邻或只隔空白），
 *       从选区终点往后找 close 标记（要求紧邻或只隔空白）。
 */
bool InlineFormatter::findWrap(const QString &id, const QString &text, int selStart, int selEnd,
                               int &openPos, int &closePos,
                               int &contentStart, int &contentEnd) const
{
    const Spec s = spec(id);
    if (s.open.isEmpty() || s.close.isEmpty()) {
        return false;
    }
    // 选区必须非空且在文本范围内
    if (selStart < 0 || selEnd > text.length() || selStart >= selEnd) {
        return false;
    }

    // ---- 往前找起始标记 ----
    int o = -1;
    for (qsizetype i = selStart - s.open.length(); i >= 0; --i) {
        if (text.mid(i, s.open.length()) == s.open) {
            o = static_cast<int>(i);
            break;
        }
    }
    if (o < 0) {
        return false;
    }
    // 起始标记与内容之间只能是空白
    const int gap1 = selStart - o - static_cast<int>(s.open.length());
    if (gap1 < 0) {
        return false;
    }
    if (!text.mid(o + s.open.length(), gap1).trimmed().isEmpty()) {
        return false;
    }

    // ---- 往后找结束标记 ----
    int c = -1;
    for (qsizetype i = selEnd; i + s.close.length() <= text.length(); ++i) {
        if (text.mid(i, s.close.length()) == s.close) {
            c = static_cast<int>(i);
            break;
        }
    }
    if (c < 0) {
        return false;
    }
    // 内容与结束标记之间只能是空白
    const int gap2 = c - selEnd;
    if (gap2 < 0) {
        return false;
    }
    if (!text.mid(selEnd, gap2).trimmed().isEmpty()) {
        return false;
    }

    openPos = o;
    closePos = c;
    contentStart = o + static_cast<int>(s.open.length());
    contentEnd = c;
    return true;
}

bool InlineFormatter::isWrapped(const QString &id, const QString &text,
                                int selStart, int selEnd) const
{
    int o = -1, c = -1, cs = -1, ce = -1;
    return findWrap(id, text, selStart, selEnd, o, c, cs, ce);
}

/**
 * 超链接是特例：开标记 "["，闭标记是 "](" + 地址 + ")"，不是固定字符串。
 * 这里单独处理。
 */
static bool toggleLink(const QString &text, int selStart, int selEnd,
                       QString &outText, int &outSelStart, int &outSelEnd,
                       const QString &placeholder)
{
    // 先找是否已包裹：[文本](地址)
    int openPos = -1;
    for (qsizetype i = selStart - 1; i >= 0; --i) {
        if (text.at(i) == QLatin1Char('[')) {
            openPos = static_cast<int>(i);
            break;
        }
    }
    if (openPos >= 0) {
        // 找 ](
        int linkStart = -1;
        for (qsizetype i = selEnd; i + 1 < text.length(); ++i) {
            if (text.at(i) == QLatin1Char(']') && text.at(i + 1) == QLatin1Char('(')) {
                linkStart = static_cast<int>(i);
                break;
            }
        }
        if (linkStart >= 0) {
            // 找到配对的 )
            int linkEnd = -1;
            for (qsizetype i = linkStart + 2; i < text.length(); ++i) {
                if (text.at(i) == QLatin1Char(')')) {
                    linkEnd = static_cast<int>(i);
                    break;
                }
            }
            if (linkEnd >= 0) {
                // 取消包裹：去掉 [ 与 ](地址)
                outText = text.left(openPos) + text.mid(openPos + 1, linkStart - openPos - 1)
                        + text.mid(linkEnd + 1);
                outSelStart = openPos;
                outSelEnd = openPos + (linkStart - openPos - 1);
                return true;
            }
        }
    }

    // 包裹
    const QString content = (selStart < selEnd)
                                ? text.mid(selStart, selEnd - selStart)
                                : placeholder;
    const QString url = (selStart < selEnd) ? QString() : QStringLiteral("https://");
    outText = text.left(selStart) + QLatin1Char('[') + content + QStringLiteral("](") + url
            + QLatin1Char(')') + text.mid(selEnd);
    outSelStart = selStart + 1;
    outSelEnd = outSelStart + static_cast<int>(content.length());
    return true;
}

bool InlineFormatter::toggle(const QString &id,
                              const QString &text, int selStart, int selEnd,
                              QString &outText, int &outSelStart, int &outSelEnd,
                              const QString &placeholder) const
{
    // ---- 边界钳制 ----
    const int len = static_cast<int>(text.length());
    if (selStart < 0) selStart = 0;
    if (selEnd > len) selEnd = len;
    if (selStart > selEnd) {
        const int t = selStart;
        selStart = selEnd;
        selEnd = t;
    }

    // ---- 超链接特例 ----
    if (id == QLatin1String("link")) {
        return toggleLink(text, selStart, selEnd, outText, outSelStart, outSelEnd, placeholder);
    }

    const Spec s = spec(id);
    if (s.open.isEmpty() || s.close.isEmpty()) {
        return false;
    }

    // ---- 情况 1：已包裹 -> 取消包裹 ----
    int o = -1, c = -1, cs = -1, ce = -1;
    if (selStart < selEnd && findWrap(id, text, selStart, selEnd, o, c, cs, ce)) {
        outText = text;
        // 先删靠后的 close，再删靠前的 open（o < c，删 close 不影响 o 的位置）
        if (c + static_cast<int>(s.close.length()) <= outText.length()) {
            outText.remove(c, static_cast<int>(s.close.length()));
        }
        if (o + static_cast<int>(s.open.length()) <= outText.length()) {
            outText.remove(o, static_cast<int>(s.open.length()));
        }
        outSelStart = o;
        outSelEnd = o + (ce - cs);
        return true;
    }

    // ---- 情况 2：有选区 -> 包裹 ----
    if (selStart < selEnd) {
        const QString content = text.mid(selStart, selEnd - selStart);
        outText = text.left(selStart) + s.open + content + s.close + text.mid(selEnd);
        outSelStart = selStart + static_cast<int>(s.open.length());
        outSelEnd = outSelStart + static_cast<int>(content.length());
        return true;
    }

    // ---- 情况 3：无选区 -> 插入模板 ----
    QString inner = placeholder;
    if (inner.isEmpty()) {
        inner = QObject::tr("文本");
    }
    outText = text.left(selStart) + s.open + inner + s.close + text.mid(selEnd);
    outSelStart = selStart + static_cast<int>(s.open.length());
    outSelEnd = outSelStart + static_cast<int>(inner.length());
    return true;
}
