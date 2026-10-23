#ifndef OUTLINEPANEL_H
#define OUTLINEPANEL_H

#include <QWidget>
#include <QTreeWidget>
#include <QStringList>
#include <QMap>

/**
 * 大纲面板（Outline）
 *
 * 对应 Typora 的「视图 > 大纲」功能（frame.js 中 outline 相关引用 300+ 处，
 * base.css 中 .outline-content / .outline-h1..h6 样式）。
 *
 * 工作方式：
 *  1. 扫描 Markdown 源码中的 ATX 标题，生成树状结构
 *  2. 点击条目跳转到编辑器对应行
 *  3. 编辑器光标移动时高亮当前所在章节
 *  4. 标题文本变化时自动去重加后缀，保证锚点唯一
 */
class OutlinePanel : public QWidget
{
    Q_OBJECT

public:
    explicit OutlinePanel(QWidget *parent = nullptr);

    // 从 Markdown 源码重建大纲
    void updateOutline(const QString &markdown);

    // 调试辅助：输出解析出的标题列表（层级 / 行号 / 文本 / 锚点）
    static void dumpOutline(const QString &markdown, QStringList &out);


    // 高亮指定行所在的标题
    void setCurrentLine(int line);

    // 清空
    void clear();

    // 主题样式
    void applyTheme(const QString &themeId);

signals:
    // 用户点击大纲条目，请求跳转到某一行（用于移动编辑器光标）
    void navigateToLine(int line);

    // 用户点击大纲条目，请求滚动预览到对应锚点（用于预览 / 预览专属模式）
    void navigateToAnchor(const QString &anchor);

    // 大纲内容变化
    void outlineChanged(int count);

protected:
    void keyPressEvent(QKeyEvent *event) override;

private slots:
    void onItemClicked(QTreeWidgetItem *item, int column);
    void onItemActivated(QTreeWidgetItem *item, int column);

private:
    // 标题项 -> 源码行号
    static void collectHeadings(const QString &markdown, QList<int> &lines,
                                QList<int> &levels, QList<QString> &texts);

    static QString makeUniqueAnchor(const QString &base, QMap<QString, int> &used);

    QTreeWidget *m_tree;
    QMap<QTreeWidgetItem*, int> m_itemLines;   // 条目 -> 行号
    QMap<QTreeWidgetItem*, QString> m_itemAnchors;
    int m_currentLine;
};

#endif // OUTLINEPANEL_H
