#ifndef MARKDOWNHIGHLIGHTER_H
#define MARKDOWNHIGHLIGHTER_H

#include <QSyntaxHighlighter>
#include <QTextCharFormat>
#include <QVector>
#include <QRegularExpression>

class QTextDocument;

class MarkdownHighlighter : public QSyntaxHighlighter
{
    Q_OBJECT

public:
    explicit MarkdownHighlighter(QTextDocument *parent = nullptr, bool darkMode = false);
    ~MarkdownHighlighter();

    void setDarkMode(bool enabled);

    /**
     * 开关语法高亮。
     *
     * QSyntaxHighlighter 没有 setEnabled，只能用「解除与文档的关联」的方式停用：
     * setDocument(nullptr) 会让 Qt 清掉已经应用到各 block 上的高亮格式，
     * 同时 highlightBlock() 不再被调用；重新 setDocument(原来的 doc) 即可恢复。
     *
     * 之前源码模式里写的是 setDocument(document()) —— 传入同一个指针，
     * Qt 内部首行就 return，等于什么都没做，高亮其实从未被关掉。
     */
    void setHighlightingEnabled(bool enabled);
    bool isHighlightingEnabled() const { return m_enabled; }

protected:
    void highlightBlock(const QString &text) override;

private:
    struct HighlightingRule {
        QRegularExpression pattern;
        QTextCharFormat format;
    };

    void setupRules();

    // 格式
    QTextCharFormat m_headingFormat;
    QTextCharFormat m_codeFormat;
    QTextCharFormat m_linkFormat;
    QTextCharFormat m_boldFormat;
    QTextCharFormat m_italicFormat;
    QTextCharFormat m_strikethroughFormat;
    QTextCharFormat m_blockquoteFormat;
    QTextCharFormat m_listMarkerFormat;
    QTextCharFormat m_codeBlockFormat;

    QVector<HighlightingRule> m_rules;
    bool m_darkMode;
    bool m_enabled = true;
    // 停用时暂存的 document —— setDocument(nullptr) 之后 document() 会返回空
    QTextDocument *m_detachedDocument = nullptr;
};

#endif // MARKDOWNHIGHLIGHTER_H