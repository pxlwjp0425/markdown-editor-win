#ifndef SMARTPUNCTUATION_H
#define SMARTPUNCTUATION_H

#include <QObject>
#include <QString>

/**
 * 智能标点转换器
 *
 * 对应 Typora 的「编辑 > 智能标点」功能：
 *   - Smart Quotes：智能引号（直引号 → 弯引号）
 *   - Smart Dashes：智能破折号（-- → em dash，--- → 全角破折号）
 *
 * Typora 的判定规则（frame.js 中 smartPunctuation 相关逻辑）：
 *   - 英文语境下 " 变 “ ”，' 变 ‘ ’
 *   - 中文语境下 "" 保持全角，'' 保持全角
 *   - 破折号：--- → ——（中文），-- → –（en dash）
 */
class SmartPunctuation : public QObject
{
public:
    // 中文标点模式
    enum Mode {
        Off,         // 关闭
        English,     // 英文模式：直引号转弯引号
        Chinese      // 中文模式：中文标点
    };

    explicit SmartPunctuation(QObject *parent = nullptr);

    void setEnabled(bool enabled);
    bool isEnabled() const { return m_enabled; }

    void setMode(Mode mode);
    Mode mode() const { return m_mode; }

    // 智能引号开关（Typora「编辑 > 智能标点 > 智能引号」）
    void setSmartQuotes(bool on);
    bool smartQuotes() const { return m_smartQuotes; }

    // 智能破折号开关（Typora「编辑 > 智能标点 > 智能破折号」）
    void setSmartDashes(bool on);
    bool smartDashes() const { return m_smartDashes; }

    /**
     * 转换单个输入字符。
     *
     * @param ch        用户输入的字符
     * @param prevChar  光标前一个字符
     * @param nextChar  光标后一个字符
     * @param replace   输出：需要替换的字符（若为空表示不替换）
     * @param caretBack 输出：替换后光标应回退的位数（用于让引号成对）
     * @return true 表示发生了转换
     */
    bool process(char ch, QChar prevChar, QChar nextChar,
                 QString &replace, int &caretBack) const;

    // 便捷方法：直接处理一段文本（用于粘贴时）
    QString convertText(const QString &text) const;

private:
    // 判断字符是否为中文（决定标点风格）
    static bool isChineseContext(QChar prev, QChar next);

    bool m_enabled;
    Mode m_mode;
    bool m_smartQuotes;
    bool m_smartDashes;
};

#endif // SMARTPUNCTUATION_H
