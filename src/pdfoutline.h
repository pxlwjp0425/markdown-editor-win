#pragma once

#include <QByteArray>
#include <QVector>
#include <QString>

/**
 * 一条 PDF 大纲（侧边栏书签）条目。
 * 由导出流程根据文档布局计算出「页码 + 页内纵坐标」后填入。
 */
struct PdfOutlineEntry {
    QString title;      // 书签显示文字（支持中文等 Unicode）
    int pageIndex = 0;  // 0 基页码
    double y = 0.0;     // 该标题在页面上的纵坐标（从页面底部起算，单位：PDF 点）
};

/**
 * 解析 PDF，返回页面对象编号（按文档顺序排列）。
 * 其 size() 即为页数。解析失败返回空。
 */
QVector<int> pdfPageObjectNumbers(const QByteArray &pdf);

/**
 * 向 PDF 字节流注入 /Outlines 大纲（阅读器左侧导航树 / 书签）。
 *
 * 做法：在原始 xref 之前追加「大纲根 + 各条目 + 新的 Catalog」对象，
 * 然后重写 xref 表与 trailer（更新 /Root 指向新 Catalog，新 Catalog 带 /Outlines）。
 * 原始内容字节完全不动，因此不会破坏已渲染的页面。
 *
 * 注意：仅针对 Qt（QTextDocument::print / QPdfWriter）生成的、使用经典 xref 表的 PDF。
 * 若 entries 为空，直接返回原 PDF（不注入空大纲）。
 */
QByteArray injectPdfOutline(const QByteArray &pdf,
                            const QVector<PdfOutlineEntry> &entries,
                            int pageCount);
