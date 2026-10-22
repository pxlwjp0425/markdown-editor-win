#ifndef MARKDOWNEDITOR_H
#define MARKDOWNEDITOR_H

#include <QMap>
#include <QString>
#include <QStringList>

/**
 * Markdown 行内格式引擎。
 *
 * 负责所有「包裹型」行内格式的切换：
 *   加粗 **  斜体 *  删除线 ~~  行内代码 `  高亮 ==  上标 ^  下标 ~  注释 <!-- -->
 *   超链接 []()  行内公式 $  脚注 [^]
 *
 * 与 Typora 的行为一致：
 *   - 有选区时：把选区包起来（已包裹则取消包裹）
 *   - 无选区时：插入带占位符的模板，并把光标停在占位符上
 *   - 取消包裹时要能正确处理嵌套与不对称的情况
 */
class InlineFormatter
{
public:
    struct Spec {
        QString open;      // 起始标记，如 "**"
        QString close;     // 结束标记，通常与 open 相同
        bool same = true;  // open == close 时走「对称包裹」逻辑
    };

    InlineFormatter();

    // 注册一个格式
    void registerFormat(const QString &id, const Spec &spec);

    // 已注册的格式 id
    QStringList formatIds() const { return m_order; }

    // 取得格式定义
    Spec spec(const QString &id) const { return m_specs.value(id); }

    /**
     * 对选区应用/取消格式。
     *
     * @param id      格式 id
     * @param text    编辑器全文
     * @param selStart 选区起点
     * @param selEnd   选区终点
     * @param outText 输出的新全文
     * @param outSelStart 新选区起点
     * @param outSelEnd   新选区终点
     * @param placeholder  无选区时插入的占位文本
     * @return true 表示发生了修改
     */
    bool toggle(const QString &id,
                const QString &text, int selStart, int selEnd,
                QString &outText, int &outSelStart, int &outSelEnd,
                const QString &placeholder = QString()) const;

    /**
     * 检测选区是否已被该格式包裹。
     * 用于「已包裹则取消」以及菜单勾选状态。
     */
    bool isWrapped(const QString &id, const QString &text, int selStart, int selEnd) const;

private:
    // 找到选区外层包裹的标记位置；未找到返回 false
    bool findWrap(const QString &id, const QString &text, int selStart, int selEnd,
                  int &openPos, int &closePos, int &contentStart, int &contentEnd) const;

    QMap<QString, Spec> m_specs;
    QStringList m_order;
};

#endif // MARKDOWNEDITOR_H
