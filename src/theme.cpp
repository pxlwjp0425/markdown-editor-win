#include "theme.h"

#include <QFile>
#include <QDir>

/**
 * 主题实现
 *
 * 所有配色取自 Typora 内置主题的 CSS 变量定义：
 *   resources/style/themes/github.css
 *   resources/style/themes/night.css
 *   resources/style/themes/newsprint.css
 *   resources/style/themes/pixyll.css
 *   resources/style/themes/whitey.css
 * 以及 base.css 中的公共变量（--monospace、--select-text-bg-color 等）。
 *
 * Typora 用 @font-face + woff2 引入 Open Sans / PT Serif / Merriweather 等字体，
 * 本项目不内嵌字体文件，改用系统等价字体，避免分发包体积膨胀。
 */

const QString Theme::ThemeId::GitHub      = QStringLiteral("github");
const QString Theme::ThemeId::GitHubDark  = QStringLiteral("github-dark");
const QString Theme::ThemeId::Night       = QStringLiteral("night");
const QString Theme::ThemeId::Newsprint   = QStringLiteral("newsprint");
const QString Theme::ThemeId::Pixyll      = QStringLiteral("pixyll");
const QString Theme::ThemeId::Whitey      = QStringLiteral("whitey");
const QString Theme::ThemeId::Default     = QStringLiteral("default");

Theme::Theme(QObject *parent)
    : QObject(parent)
{
}

QStringList Theme::availableThemes()
{
    return QStringList() << ThemeId::Default
                         << ThemeId::GitHub
                         << ThemeId::GitHubDark
                         << ThemeId::Night
                         << ThemeId::Newsprint
                         << ThemeId::Pixyll
                         << ThemeId::Whitey;
}

Theme::Palette Theme::defaultPalette()
{
    Palette p;
    p.id = ThemeId::Default;
    p.displayName = QObject::tr("默认主题");
    p.author = QStringLiteral("Markdown Editor");
    p.background = QColor("#ffffff");
    p.text = QColor("#333333");
    p.primary = QColor("#428bca");
    p.link = QColor("#3498db");
    p.codeBackground = QColor("#f5f5f5");
    p.codeText = QColor("#333333");
    p.border = QColor("#dddddd");
    p.quoteBackground = QColor("#f9f9f9");
    p.quoteText = QColor("#666666");
    p.selectionBackground = QColor("#b4d5fe");
    p.heading = QColor("#2c3e50");
    p.secondaryText = QColor("#8e44ad");
    p.codeBlockBorder = QColor("#dddddd");
    p.fontFamily = QStringLiteral("'Segoe UI', 'Microsoft YaHei', 'PingFang SC', sans-serif");
    p.monoFontFamily = QStringLiteral("'Consolas', 'Courier New', monospace");
    p.fontSize = 16;
    p.lineHeight = 160;
    p.contentWidth = 860;
    p.dark = false;
    return p;
}

Theme::Palette Theme::githubPalette()
{
    Palette p = defaultPalette();
    p.id = ThemeId::GitHub;
    p.displayName = QObject::tr("GitHub");
    p.author = QStringLiteral("Typora / GitHub");
    p.background = QColor("#ffffff");
    p.text = QColor("#333333");
    p.primary = QColor("#428bca");
    p.link = QColor("#4183c4");
    p.codeBackground = QColor("#f6f8fa");
    p.codeText = QColor("#333333");
    p.border = QColor("#eaecef");
    p.quoteBackground = QColor("#f6f8fa");
    p.quoteText = QColor("#6a737d");
    p.selectionBackground = QColor("#b4d5fe");
    p.heading = QColor("#333333");
    p.secondaryText = QColor("#6a737d");
    p.codeBlockBorder = QColor("#e1e4e8");
    p.fontFamily = QStringLiteral("'Open Sans', 'Segoe UI', 'Microsoft YaHei', sans-serif");
    p.monoFontFamily = QStringLiteral("'Consolas', 'Courier New', monospace");
    p.contentWidth = 860;
    return p;
}

Theme::Palette Theme::githubDarkPalette()
{
    Palette p = defaultPalette();
    p.id = ThemeId::GitHubDark;
    p.displayName = QObject::tr("GitHub 深色");
    p.author = QStringLiteral("Typora / GitHub");
    p.background = QColor("#0d1117");
    p.text = QColor("#c9d1d9");
    p.primary = QColor("#58a6ff");
    p.link = QColor("#58a6ff");
    p.codeBackground = QColor("#161b22");
    p.codeText = QColor("#c9d1d9");
    p.border = QColor("#30363d");
    p.quoteBackground = QColor("#161b22");
    p.quoteText = QColor("#8b949e");
    p.selectionBackground = QColor("#264f78");
    p.heading = QColor("#58a6ff");
    p.secondaryText = QColor("#8b949e");
    p.codeBlockBorder = QColor("#30363d");
    p.contentWidth = 860;
    p.dark = true;
    return p;
}

Theme::Palette Theme::nightPalette()
{
    // 取自 Typora night.css 的 :root 变量
    Palette p = defaultPalette();
    p.id = ThemeId::Night;
    p.displayName = QObject::tr("Night");
    p.author = QStringLiteral("Typora");
    p.background = QColor("#363B40");
    p.text = QColor("#b8bfc6");
    p.primary = QColor("#6dc1e7");
    p.link = QColor("#6dc1e7");
    p.codeBackground = QColor("#2E3033");
    p.codeText = QColor("#b8bfc6");
    p.border = QColor("#555555");
    p.quoteBackground = QColor("#2E3033");
    p.quoteText = QColor("#8f9192");
    p.selectionBackground = QColor("#4a89dc");
    p.heading = QColor("#6dc1e7");
    p.secondaryText = QColor("#70717d");
    p.codeBlockBorder = QColor("#555555");
    p.fontFamily = QStringLiteral("'Open Sans', 'Segoe UI', 'Microsoft YaHei', sans-serif");
    p.contentWidth = 914;
    p.dark = true;
    return p;
}

Theme::Palette Theme::newsprintPalette()
{
    // 取自 Typora newsprint.css —— 报刊风格，衬线字体
    Palette p = defaultPalette();
    p.id = ThemeId::Newsprint;
    p.displayName = QObject::tr("Newsprint 报刊");
    p.author = QStringLiteral("Typora");
    p.background = QColor("#fffff8");
    p.text = QColor("#333333");
    p.primary = QColor("#4a4a4a");
    p.link = QColor("#4a4a4a");
    p.codeBackground = QColor("#f8f8f8");
    p.codeText = QColor("#333333");
    p.border = QColor("#dddddd");
    p.quoteBackground = QColor("#f8f8f8");
    p.quoteText = QColor("#555555");
    p.selectionBackground = QColor("#b4d5fe");
    p.heading = QColor("#333333");
    p.secondaryText = QColor("#777777");
    p.codeBlockBorder = QColor("#dddddd");
    // Typora 用 PT Serif，这里用系统衬线字体等价替代
    p.fontFamily = QStringLiteral("'Georgia', 'Times New Roman', 'SimSun', serif");
    p.monoFontFamily = QStringLiteral("'Consolas', 'Courier New', monospace");
    p.contentWidth = 700;
    return p;
}

Theme::Palette Theme::pixyllPalette()
{
    // 取自 Typora pixyll.css —— 源自 pixyll.com
    Palette p = defaultPalette();
    p.id = ThemeId::Pixyll;
    p.displayName = QObject::tr("Pixyll");
    p.author = QStringLiteral("Typora / pixyll.com");
    p.background = QColor("#ffffff");
    p.text = QColor("#333333");
    p.primary = QColor("#3e8338");
    p.link = QColor("#3e8338");
    p.codeBackground = QColor("#f6f6f6");
    p.codeText = QColor("#333333");
    p.border = QColor("#eeeeee");
    p.quoteBackground = QColor("#f6f6f6");
    p.quoteText = QColor("#555555");
    p.selectionBackground = QColor("#b4d5fe");
    p.heading = QColor("#3e8338");
    p.secondaryText = QColor("#727272");
    p.codeBlockBorder = QColor("#eeeeee");
    // Typora 用 Merriweather，这里用系统衬线字体等价替代
    p.fontFamily = QStringLiteral("'Georgia', 'Times New Roman', 'SimSun', serif");
    p.contentWidth = 800;
    return p;
}

Theme::Palette Theme::whiteyPalette()
{
    // 取自 Typora whitey.css —— 极简无衬线
    Palette p = defaultPalette();
    p.id = ThemeId::Whitey;
    p.displayName = QObject::tr("Whitey 极简");
    p.author = QStringLiteral("Typora");
    p.background = QColor("#ffffff");
    p.text = QColor("#4a4a4a");
    p.primary = QColor("#4a4a4a");
    p.link = QColor("#4a4a4a");
    p.codeBackground = QColor("#f7f7f7");
    p.codeText = QColor("#4a4a4a");
    p.border = QColor("#eeeeee");
    p.quoteBackground = QColor("#fafafa");
    p.quoteText = QColor("#777777");
    p.selectionBackground = QColor("#b4d5fe");
    p.heading = QColor("#4a4a4a");
    p.secondaryText = QColor("#999999");
    p.codeBlockBorder = QColor("#eeeeee");
    p.fontFamily = QStringLiteral("'Segoe UI', 'Helvetica Neue', 'Microsoft YaHei', sans-serif");
    p.contentWidth = 780;
    return p;
}

Theme::Palette Theme::palette(const QString &themeId)
{
    if (themeId == ThemeId::GitHub)     return githubPalette();
    if (themeId == ThemeId::GitHubDark) return githubDarkPalette();
    if (themeId == ThemeId::Night)      return nightPalette();
    if (themeId == ThemeId::Newsprint)  return newsprintPalette();
    if (themeId == ThemeId::Pixyll)     return pixyllPalette();
    if (themeId == ThemeId::Whitey)     return whiteyPalette();
    return defaultPalette();
}

QColor Theme::accentColor(const QString &themeId)
{
    return palette(themeId).primary;
}

QString Theme::previewCss(const QString &themeId)
{
    const Palette p = palette(themeId);

    // class 名沿用 Typora 的语义，便于对照理解
    // 顶部输出 CSS 变量，语义与 Typora 的 :root 变量一致，
    // 用户可直接改写这些变量来微调配色。
    return QString(
        ":root {\n"
        "  --bg-color: %3;\n"
        "  --text-color: %4;\n"
        "  --primary-color: %7;\n"
        "  --link-color: %10;\n"
        "  --code-bg-color: %11;\n"
        "  --code-text-color: %12;\n"
        "  --border-color: %8;\n"
        "  --select-text-bg-color: %17;\n"
        "}\n"
        "html { font-size: %1px; -webkit-font-smoothing: antialiased; }\n"
        "body {\n"
        "  font-family: %2;\n"
        "  background-color: %3;\n"
        "  color: %4;\n"
        "  padding: 20px 24px;\n"
        "  line-height: %5;\n"
        "}\n"
        // Qt 的 QTextBrowser：
        //   - 不支持 margin:auto，无法用 CSS 自动居中
        //   - 忽略 body 的 padding
        //   - div 上的 margin-left/right 也不生效
        // 因此居中留白改由 Qt 原生 API 实现：
        // MainWindow 在 setHtml 后设置 QTextDocument 根 frame 的左右边距
        // （见 MainWindow::applyPreviewCentering）。
        ".markdown-body {\n"
        "  max-width: %6px;\n"
        "  text-align: left;\n"
        "}\n"
        "h1, h2, h3, h4, h5, h6 {\n"
        "  color: %7;\n"
        "  margin-top: 24px;\n"
        "  margin-bottom: 16px;\n"
        "  font-weight: 600;\n"
        "  line-height: 1.25;\n"
        "}\n"
        "h1 { font-size: 2em; border-bottom: 1px solid %8; padding-bottom: 0.3em; }\n"
        "h2 { font-size: 1.5em; border-bottom: 1px solid %8; padding-bottom: 0.3em; }\n"
        "h3 { font-size: 1.25em; }\n"
        "h4 { font-size: 1em; }\n"
        "h5 { font-size: 0.875em; }\n"
        "h6 { font-size: 0.85em; color: %9; }\n"
        "p { margin-top: 0; margin-bottom: 16px; }\n"
        "a { color: %10; text-decoration: none; }\n"
        "a:hover { text-decoration: underline; }\n"
        "code {\n"
        "  background-color: %11;\n"
        "  color: %12;\n"
        "  padding: 0.2em 0.4em;\n"
        "  border-radius: 3px;\n"
        "  font-family: %13;\n"
        "  font-size: 0.9em;\n"
        "}\n"
        "pre {\n"
        "  background-color: %11;\n"
        "  color: %12;\n"
        "  padding: 16px;\n"
        "  border: 1px solid %14;\n"
        "  border-radius: 6px;\n"
        "  overflow: auto;\n"
        "  line-height: 1.45;\n"
        "}\n"
        "pre code { background: none; padding: 0; border: none; font-size: 0.875em; }\n"
        "blockquote {\n"
        "  border-left: 4px solid %15;\n"
        "  padding: 0 15px;\n"
        "  color: %16;\n"
        "  margin: 1rem 0;\n"
        "}\n"
        "blockquote > :first-child { margin-top: 0; }\n"
        "blockquote > :last-child { margin-bottom: 0; }\n"
        "ul, ol { padding-left: 2em; }\n"
        "li { margin: 0.25em 0; }\n"
        "table {\n"
        "  border-collapse: collapse;\n"
        "  width: 100%%;\n"
        "  margin: 16px 0;\n"
        "  overflow: auto;\n"
        "}\n"
        "th, td { border: 1px solid %8; padding: 8px 12px; }\n"
        "th { background-color: %11; font-weight: 600; }\n"
        "tr:nth-child(2n) { background-color: %17; }\n"
        "hr { border: none; border-top: 1px solid %8; margin: 24px 0; }\n"
        "img { max-width: 100%%; vertical-align: middle; }\n"
        "mark { background: #ff0; color: #000; }\n"
        "del { color: %9; }\n"
        /* 任务列表 —— 对应 Typora 的 .md-task-list-item */
        ".md-task-list-item { list-style-type: none; }\n"
        ".md-task-list-item .md-task-box { text-decoration: none; margin-right: 0.4em; cursor: pointer; }\n"
        "// 目录 —— 对应 Typora 的 .md-toc\n"
        ".md-toc {\n"
        "  min-height: 3.58rem;\n"
        "  position: relative;\n"
        "  font-size: 0.9rem;\n"
        "  border-radius: 10px;\n"
        "  background-color: %11;\n"
        "  padding: 12px 16px;\n"
        "  margin: 16px 0;\n"
        "}\n"
        ".md-toc-content { position: relative; margin-left: 0; }\n"
        ".md-toc-item { display: block; color: %10; }\n"
        ".md-toc-item a { text-decoration: none; }\n"
        ".md-toc-h1 .md-toc-inner { margin-left: 0; font-weight: 700; }\n"
        ".md-toc-h2 .md-toc-inner { margin-left: 2em; }\n"
        ".md-toc-h3 .md-toc-inner { margin-left: 4em; }\n"
        ".md-toc-h4 .md-toc-inner { margin-left: 6em; }\n"
        ".md-toc-h5 .md-toc-inner { margin-left: 8em; }\n"
        ".md-toc-h6 .md-toc-inner { margin-left: 10em; }\n"
        /* 数学公式 —— 对应 Typora 的 .md-math-block / .md-inline-math-container */
        ".md-math-block { width: 100%%; text-align: center; margin: 1.2em 0; }\n"
        ".md-math-block .md-math-content { font-family: 'Cambria Math', 'Latin Modern Math', serif; font-size: 1.05em; }\n"
        ".md-inline-math-container { display: inline; }\n"
        ".md-inline-math-container .md-math-content { font-family: 'Cambria Math', 'Latin Modern Math', serif; }\n"
        ".md-math-frac { display: inline-block; vertical-align: middle; text-align: center; margin: 0 0.15em; }\n"
        ".md-math-frac .md-math-num { display: block; border-bottom: 1px solid currentColor; padding: 0 0.25em; }\n"
        ".md-math-frac .md-math-den { display: block; padding: 0 0.25em; }\n"
        ".md-math-sup { font-size: 0.75em; vertical-align: super; }\n"
        ".md-math-sub { font-size: 0.75em; vertical-align: sub; }\n"
        ".md-math-sqrt { border-top: 1px solid currentColor; padding: 0 0.15em; }\n"
        ".md-math-vec { text-decoration: overline; font-style: italic; }\n"
        /* 图表占位（无 mermaid 引擎时的降级显示） */
        ".md-diagram-panel {\n"
        "  border: 1px dashed %8;\n"
        "  border-radius: 6px;\n"
        "  padding: 12px 16px;\n"
        "  margin: 16px 0;\n"
        "  color: %9;\n"
        "  font-family: %13;\n"
        "  font-size: 0.85em;\n"
        "}\n"
        ".md-diagram-panel .md-diagram-type { font-weight: 600; color: %10; }\n"
        "// 脚注\n"
        ".footnotes { opacity: 0.8; font-size: 0.9em; margin-top: 1em; }\n"
        "// YAML front matter\n"
        "pre.md-meta-block {\n"
        "  font-size: 0.85em;\n"
        "  min-height: 1.8em;\n"
        "  background-color: %11;\n"
        "  color: %9;\n"
        "  display: block;\n"
        "  padding: 12px 16px;\n"
        "  border: 1px solid %14;\n"
        "  border-radius: 6px;\n"
        "  margin-bottom: 16px;\n"
        "}\n"
    ).arg(p.fontSize)
     .arg(p.fontFamily)
     .arg(p.background.name())
     .arg(p.text.name())
     .arg(p.lineHeight / 100.0)
     .arg(p.contentWidth)
     .arg(p.heading.name())
     .arg(p.border.name())
     .arg(p.secondaryText.name())
     .arg(p.link.name())
     .arg(p.codeBackground.name())
     .arg(p.codeText.name())
     .arg(p.monoFontFamily)
     .arg(p.codeBlockBorder.name())
     .arg(p.border.name())
     .arg(p.quoteText.name())
     .arg(p.dark ? p.codeBackground.name() : QString("#fbfbfb"));
}

QString Theme::editorStyleSheet(const QString &themeId)
{
    const Palette p = palette(themeId);
    return QString(
        "QPlainTextEdit {"
        "  background-color: %1;"
        "  color: %2;"
        "  border: 1px solid %3;"
        "  selection-background-color: %4;"
        "  selection-color: %5;"
        "  font-family: %6;"
        "}"
    ).arg(p.background.name())
     .arg(p.text.name())
     .arg(p.codeBlockBorder.name())
     .arg(p.selectionBackground.name())
     .arg(p.dark ? QString("#ffffff") : QString("#000000"))
     .arg(p.monoFontFamily);
}

QString Theme::outlineStyleSheet(const QString &themeId)
{
    const Palette p = palette(themeId);
    // 注意：Qt 的 QSS 不支持 QTreeView::item 的 height / padding 属性
    // （见 Qt 样式表参考：仅 background/border/color/font/margin 等生效）。
    // 若给 item 设置 height，项会被压缩为 0 高度，整棵树看起来是空白的。
    // 因此行高用 margin 表达，字号放在 QTreeWidget 上。
    return QString(
        "QTreeWidget {"
        "  background-color: %1;"
        "  color: %2;"
        "  border: none;"
        "  font-family: %3;"
        "  font-size: 13px;"
        "  outline: none;"
        "  show-decoration-selected: 1;"
        "}"
        "QTreeWidget::item {"
        "  border: none;"
        "  margin: 2px 0px 2px 0px;"
        "  padding: 0px;"
        "}"
        "QTreeWidget::item:hover {"
        "  background-color: %4;"
        "}"
        "QTreeWidget::item:selected {"
        "  background-color: %5;"
        "  color: %6;"
        "}"
        "QTreeWidget::branch {"
        "  border-image: none;"
        "  image: none;"
        "}"
    ).arg(p.background.name())
     .arg(p.text.name())
     .arg(p.fontFamily)
     .arg(p.dark ? QString("#2E3033") : QString("#f5f5f5"))
     .arg(p.selectionBackground.name())
     .arg(p.dark ? QString("#ffffff") : QString("#000000"));
}
