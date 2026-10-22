#include "markdownhighlighter.h"

MarkdownHighlighter::MarkdownHighlighter(QTextDocument *parent, bool darkMode)
    : QSyntaxHighlighter(parent)
    , m_darkMode(darkMode)
{
    setupRules();
}

MarkdownHighlighter::~MarkdownHighlighter()
{
}

void MarkdownHighlighter::setDarkMode(bool enabled)
{
    m_darkMode = enabled;
    setupRules();
    rehighlight();
}

/**
 * 开关语法高亮。
 *
 * 停用：setDocument(nullptr) —— Qt 会遍历所有 block 清掉已应用的高亮格式，
 * 并断开文档的 contentsChange / reformatBlock 信号，之后不再回调 highlightBlock()。
 * 启用：把暂存的 document 重新挂回去，Qt 会自动触发一次全量 rehighlight。
 */
void MarkdownHighlighter::setHighlightingEnabled(bool enabled)
{
    if (m_enabled == enabled) {
        return;
    }
    m_enabled = enabled;

    if (!enabled) {
        // 必须先记住 document：setDocument(nullptr) 之后 document() 就返回空了
        m_detachedDocument = document();
        if (m_detachedDocument) {
            setDocument(nullptr);
        }
    } else if (m_detachedDocument) {
        setDocument(m_detachedDocument);
        m_detachedDocument = nullptr;
    }
}

void MarkdownHighlighter::setupRules()
{
    if (m_darkMode) {
        // 深色模式颜色
        m_headingFormat.setForeground(QColor("#569cd6"));
        m_headingFormat.setFontWeight(QFont::Bold);
        
        m_codeFormat.setBackground(QColor("#2d2d2d"));
        m_codeFormat.setForeground(QColor("#ce9178"));
        
        m_linkFormat.setForeground(QColor("#4ec9b0"));
        m_linkFormat.setUnderlineStyle(QTextCharFormat::SingleUnderline);
        
        m_boldFormat.setFontWeight(QFont::Bold);
        m_boldFormat.setForeground(QColor("#d4d4d4"));
        
        m_italicFormat.setFontItalic(true);
        m_italicFormat.setForeground(QColor("#d4d4d4"));
        
        m_strikethroughFormat.setForeground(QColor("#808080"));
        
        m_blockquoteFormat.setForeground(QColor("#6a9955"));
        m_blockquoteFormat.setBackground(QColor("#2d2d2d"));
        
        m_listMarkerFormat.setForeground(QColor("#c586c0"));
        
        m_codeBlockFormat.setBackground(QColor("#1e1e1e"));
        m_codeBlockFormat.setForeground(QColor("#d4d4d4"));
    } else {
        // 浅色模式颜色
        m_headingFormat.setForeground(QColor("#2c3e50"));
        m_headingFormat.setFontWeight(QFont::Bold);
        
        m_codeFormat.setBackground(QColor("#f5f5f5"));
        m_codeFormat.setForeground(QColor("#e74c3c"));
        
        m_linkFormat.setForeground(QColor("#3498db"));
        m_linkFormat.setUnderlineStyle(QTextCharFormat::SingleUnderline);
        
        m_boldFormat.setFontWeight(QFont::Bold);
        m_boldFormat.setForeground(QColor("#333333"));
        
        m_italicFormat.setFontItalic(true);
        m_italicFormat.setForeground(QColor("#333333"));
        
        m_strikethroughFormat.setForeground(QColor("#95a5a6"));
        
        m_blockquoteFormat.setForeground(QColor("#27ae60"));
        m_blockquoteFormat.setBackground(QColor("#f9f9f9"));
        
        m_listMarkerFormat.setForeground(QColor("#8e44ad"));
        
        m_codeBlockFormat.setBackground(QColor("#f5f5f5"));
        m_codeBlockFormat.setForeground(QColor("#333333"));
    }

    // 行锚定规则需要 MultilineOption：
    // QRegExp 的 ^ $ 默认按行边界匹配，而 QRegularExpression 默认按整串匹配。
    // 移植时若不加此选项，标题/列表/引用在编辑器里完全不高亮。
    const auto ml = QRegularExpression::MultilineOption;

    // 标题规则
    QRegularExpression heading1("^# .+$", ml);
    QRegularExpression heading2("^## .+$", ml);
    QRegularExpression heading3("^### .+$", ml);
    QRegularExpression heading4("^#### .+$", ml);
    QRegularExpression heading5("^##### .+$", ml);
    QRegularExpression heading6("^###### .+$", ml);
    
    // 代码块规则
    QRegularExpression codeBlockStart("^```", ml);
    QRegularExpression codeBlockEnd("^```$", ml);
    
    // 行内代码
    QRegularExpression inlineCode("`[^`]+`");
    
    // 链接
    QRegularExpression link("\\[[^\\]]+\\]\\([^)]+\\)");
    QRegularExpression image("!\\[([^\\]]*)\\]\\(([^)]+)\\)");
    
    // 粗体和斜体
    QRegularExpression bold("\\*\\*[^*]+\\*\\*");
    QRegularExpression boldAlt("__[^_]+__");
    QRegularExpression italic("\\*[^*]+\\*");
    QRegularExpression italicAlt("_[^_]+_");
    QRegularExpression strikethrough("~~[^~]+~~");
    
    // 列表标记
    QRegularExpression bulletList("^\\s*[-*+]\\s", ml);
    QRegularExpression numberedList("^\\s*\\d+\\.\\s", ml);
    
    // 引用
    QRegularExpression blockquote("^>\\s", ml);
    
    // 添加规则
    m_rules.clear();
    m_rules.append({heading1, m_headingFormat});
    m_rules.append({heading2, m_headingFormat});
    m_rules.append({heading3, m_headingFormat});
    m_rules.append({heading4, m_headingFormat});
    m_rules.append({heading5, m_headingFormat});
    m_rules.append({heading6, m_headingFormat});
    m_rules.append({codeBlockStart, m_codeBlockFormat});
    m_rules.append({codeBlockEnd, m_codeBlockFormat});
    m_rules.append({inlineCode, m_codeFormat});
    m_rules.append({link, m_linkFormat});
    m_rules.append({image, m_linkFormat});
    m_rules.append({bold, m_boldFormat});
    m_rules.append({boldAlt, m_boldFormat});
    m_rules.append({italic, m_italicFormat});
    m_rules.append({italicAlt, m_italicFormat});
    m_rules.append({strikethrough, m_strikethroughFormat});
    m_rules.append({bulletList, m_listMarkerFormat});
    m_rules.append({numberedList, m_listMarkerFormat});
    m_rules.append({blockquote, m_blockquoteFormat});
}

void MarkdownHighlighter::highlightBlock(const QString &text)
{
    for (const HighlightingRule &rule : m_rules) {
        QRegularExpressionMatchIterator matchIterator = rule.pattern.globalMatch(text);
        while (matchIterator.hasNext()) {
            QRegularExpressionMatch match = matchIterator.next();
            setFormat(match.capturedStart(), match.capturedLength(), rule.format);
        }
    }
}