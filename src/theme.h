#ifndef THEME_H
#define THEME_H

#include <QObject>
#include <QString>
#include <QStringList>
#include <QColor>

/**
 * 主题管理器
 *
 * 设计源自 Typora 的主题体系（resources/style/themes 目录下的 css 文件）：
 * Typora 用 CSS 变量定义配色（--bg-color / --text-color / --primary-color ...），
 * 本类把同一套变量模型映射为 C++ 结构，并为预览区生成对应的 CSS。
 *
 * 内置主题对应 Typora 的 github / night / newsprint / pixyll / whitey，
 * 额外提供 github-dark 与 typora-default 两个变体。
 */
class Theme : public QObject
{
public:
    // 主题标识
    struct ThemeId {
        static const QString GitHub;
        static const QString GitHubDark;
        static const QString Night;
        static const QString Newsprint;
        static const QString Pixyll;
        static const QString Whitey;
        static const QString Default;
    };

    // 单个主题的配色集合 —— 对应 Typora 的 :root CSS 变量
    struct Palette {
        QString id;                 // 内部标识
        QString displayName;        // 显示名
        QString author;             // 主题作者（对应 Typora 主题署名）

        QColor background;          // --bg-color
        QColor text;                // --text-color
        QColor primary;             // --primary-color
        QColor link;                // 链接色
        QColor codeBackground;      // 代码块背景
        QColor codeText;            // 代码文字
        QColor border;              // 分割线 / 表格边框
        QColor quoteBackground;     // 引用背景
        QColor quoteText;           // 引用文字
        QColor selectionBackground; // --select-text-bg-color
        QColor heading;             // 标题色
        QColor secondaryText;       // 弱化文字（目录、注释）
        QColor codeBlockBorder;

        QString fontFamily;         // 正文字体
        QString monoFontFamily;     // 等宽字体
        int fontSize;               // 基准字号 px
        int lineHeight;             // 行高倍数 * 100
        int contentWidth;           // 正文最大宽度 px
        bool dark;                  // 是否为深色主题
    };

    explicit Theme(QObject *parent = nullptr);

    // 可用主题列表（按显示顺序）
    static QStringList availableThemes();

    // 取主题配色；未知 id 回退到 Default
    static Palette palette(const QString &themeId);

    // 生成预览区使用的完整 CSS
    static QString previewCss(const QString &themeId);

    // 生成编辑器（QPlainTextEdit）使用的 QSS
    static QString editorStyleSheet(const QString &themeId);

    // 生成大纲面板的 QSS
    static QString outlineStyleSheet(const QString &themeId);

    // 主题对应的应用图标/强调色，用于状态栏等
    static QColor accentColor(const QString &themeId);

private:
    static Palette defaultPalette();
    static Palette githubPalette();
    static Palette githubDarkPalette();
    static Palette nightPalette();
    static Palette newsprintPalette();
    static Palette pixyllPalette();
    static Palette whiteyPalette();
};

#endif // THEME_H
