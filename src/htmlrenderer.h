#ifndef HTMLRENDERER_H
#define HTMLRENDERER_H

#include <QList>
#include <QString>
#include <QStringList>

#include "docmodel.h"

/**
 * Markdown IR -> HTML 渲染器。
 *
 * 预览区与「导出为 HTML」都走这里 —— 它们必须给出完全一样的结果，
 * 否则用户在预览里看到的样子和导出的文件对不上。
 *
 * 输入是 DocModel::parse() 的输出，不再自己解析 Markdown：
 * 一份文档只允许有一套解析规则。早期预览区有一套独立的正则状态机，
 * 与 DocModel 对同一份文档给出不同结果（表格多出空列、
 * 行内代码里的 ** 变成真粗体、~~~ 代码块失效等等）。
 */
class HtmlRenderer
{
public:
    struct Heading {
        int level = 1;        // 1~6
        QString text;         // 标题原文（未去掉行内标记）
        QString anchor;       // 唯一锚点 id
        int sourceLine = -1;  // 源码行号（0 基）
    };

    /**
     * 渲染 HTML body 片段。
     *
     * 只输出 body 内容（不含 <html> / <head> / <style>），
     * 便于预览区（QTextBrowser 不认 <style>）与整页导出共用。
     */
    static QString renderBody(const QList<DocBlock> &blocks);

    /**
     * 收集标题与锚点（供 [TOC] 使用）。
     *
     * 锚点规则与 DocModel::anchorBase / uniqueAnchor 一致，
     * 因此目录里的链接与标题上的 id 必然对得上。
     */
    static QList<Heading> collectHeadings(const QList<DocBlock> &blocks);

    /** 渲染行内片段 */
    static QString renderInlines(const QList<DocInline> &inlines);

    /** 文本转义（& < >） */
    static QString escapeHtml(const QString &text);

    /** 属性值转义（& < > "） */
    static QString escapeAttr(const QString &text);

private:
    static QString renderToc(const QList<Heading> &headings);
};

#endif // HTMLRENDERER_H
