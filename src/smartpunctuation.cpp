#include "smartpunctuation.h"

#include <QRegularExpression>

SmartPunctuation::SmartPunctuation(QObject *parent)
    : QObject(parent)
    , m_enabled(false)
    , m_mode(English)
    , m_smartQuotes(true)
    , m_smartDashes(true)
{
}

void SmartPunctuation::setEnabled(bool enabled)
{
    m_enabled = enabled;
}

void SmartPunctuation::setSmartQuotes(bool on)
{
    m_smartQuotes = on;
}

void SmartPunctuation::setSmartDashes(bool on)
{
    m_smartDashes = on;
}

void SmartPunctuation::setMode(Mode mode)
{
    m_mode = mode;
}

/**
 * 判断是否处于中文语境。
 *
 * Typora 的判定：光标前后有中文字符时，使用中文标点。
 */
bool SmartPunctuation::isChineseContext(QChar prev, QChar next)
{
    auto isCjk = [](QChar c) {
        const ushort u = c.unicode();
        // CJK 统一表意文字、中日韩标点、全角符号
        return (u >= 0x4E00 && u <= 0x9FFF) ||
               (u >= 0x3400 && u <= 0x4DBF) ||
               (u >= 0x3000 && u <= 0x303F) ||
               (u >= 0xFF00 && u <= 0xFFEF);
    };
    return isCjk(prev) || isCjk(next);
}

bool SmartPunctuation::process(char ch, QChar prevChar, QChar nextChar,
                              QString &replace, int &caretBack) const
{
    replace.clear();
    caretBack = 0;

    if (!m_enabled || m_mode == Off) {
        return false;
    }

    const QChar c(ch);

    // ---- 智能引号 ----
    if (c == '"' && m_smartQuotes) {
        // 判断是开引号还是闭引号：
        //  前一个字符是空白/行首 -> 开引号；否则 -> 闭引号
        const bool opening = prevChar.isSpace() || prevChar.isNull() ||
                             prevChar == QChar('(') || prevChar == QChar('[') ||
                             prevChar == QChar('{') || prevChar == QChar(0x201C);

        if (m_mode == Chinese || isChineseContext(prevChar, nextChar)) {
            replace = opening ? QStringLiteral("“") : QStringLiteral("”");
        } else {
            replace = opening ? QStringLiteral("“") : QStringLiteral("”");
        }
        return true;
    }

    if (c == '\'') {
        const bool opening = prevChar.isSpace() || prevChar.isNull() ||
                             prevChar == QChar('(') || prevChar == QChar('[') ||
                             prevChar == QChar('{') || prevChar == QChar(0x2018);

        // 撇号场景：don't、it's 这类不应转换
        // 判定：前后都是字母时视为英文缩写
        if (prevChar.isLetter() && nextChar.isLetter()) {
            return false;
        }

        if (m_mode == Chinese || isChineseContext(prevChar, nextChar)) {
            replace = opening ? QStringLiteral("‘") : QStringLiteral("’");
        } else {
            replace = opening ? QStringLiteral("‘") : QStringLiteral("’");
        }
        return true;
    }

    // ---- 智能破折号 ----
    // 需要看到两个连续的 '-'
    if (c == '-' && m_smartDashes) {
        if (prevChar == QChar('-')) {
            // 第二个 '-'：判断是否为三个连续
            if (nextChar == QChar('-')) {
                replace = QStringLiteral("—");   // em dash
                caretBack = 1;                     // 吃掉一个已有的 '-'
            } else {
                replace = QStringLiteral("–");   // en dash
                caretBack = 1;
            }
            return true;
        }
    }

    // ---- 省略号：... → … ----
    if (c == '.' && prevChar == QChar('.') && m_smartDashes) {
        if (nextChar == QChar('.')) {
            replace = QStringLiteral("…");
            caretBack = 1;
            return true;
        }
    }

    return false;
}

QString SmartPunctuation::convertText(const QString &text) const
{
    if (!m_enabled || m_mode == Off || text.isEmpty()) {
        return text;
    }

    QString result;
    result.reserve(text.size());

    for (int i = 0; i < text.size(); ++i) {
        const QChar c = text.at(i);
        const QChar prev = i > 0 ? text.at(i - 1) : QChar();
        const QChar next = i + 1 < text.size() ? text.at(i + 1) : QChar();

        QString replacement;
        int caretBack = 0;
        if (process(c.toLatin1(), prev, next, replacement, caretBack)) {
            if (caretBack > 0) {
                // 合并标记：前一个字符（'-' 或 '.'）已经写进 result 了，
                // 要把它换成替换字符，而不是把前一个留着、把后一个吃掉。
                //
                // 早期实现是「追加替换字符 + i += caretBack」，
                // 结果 "a--b" 变成 "a-–b"（前一个 '-' 留着，后面的 'b' 没了）。
                if (!result.isEmpty()) {
                    result.chop(caretBack);
                }
                result += replacement;
                // 三个连续的 '-' 合成一个 em dash，三个 '.' 合成一个省略号：
                // 第三个字符要跳过，否则会多出一个残留字符。
                if (next == c) {
                    ++i;
                }
            } else {
                result += replacement;
            }
        } else {
            result += c;
        }
    }

    return result;
}
