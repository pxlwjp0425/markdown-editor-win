#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""追加 exportTo 及 PDF/ODT/图片 的落盘实现到 mainwindow.cpp。"""
import io

p = 'src/mainwindow.cpp'
s = io.open(p, encoding='utf-8').read()

IMPL = r'''

// ===========================================================================
// 多格式导出
// ===========================================================================

/**
 * 统一导出入口。
 *
 * 按 Exporter::formats() 里的 id 分发：
 *   - 文本类（markdown/plaintext/latex/rtf/opml/wiki/rst/textile/html/docx/epub）
 *     由 Exporter::generateText 生成字节流后写盘
 *   - 需要打印/渲染上下文的（pdf/odt/image）单独处理
 */
void MainWindow::exportTo(const QString &formatId)
{
    QTextEdit *editor = currentEditor();
    if (!editor) {
        return;
    }

    const Exporter::Format fmt = Exporter::formatById(formatId);
    if (fmt.id.isEmpty()) {
        return;
    }

    // 默认文件名：当前文件名 + 目标扩展名
    QString baseName = tr("未命名");
    const QString curPath = editor->property("filePath").toString();
    if (!curPath.isEmpty()) {
        baseName = QFileInfo(curPath).completeBaseName();
    }
    const QString defaultPath = QDir(m_currentFolder).filePath(
        baseName + QLatin1Char('.') + fmt.defaultExt);

    QString fileName = QFileDialog::getSaveFileName(
        this, tr("导出为 %1").arg(fmt.displayName),
        defaultPath, fmt.filter);

    if (fileName.isEmpty()) {
        return;
    }
    // 补扩展名
    const QFileInfo fi(fileName);
    if (fi.suffix().isEmpty()) {
        fileName += QLatin1Char('.') + fmt.defaultExt;
    }

    // 需要特殊上下文的格式
    if (formatId == QLatin1String("pdf")) {
        exportToPdfFile(fileName);
        return;
    }
    if (formatId == QLatin1String("odt")) {
        exportToOdtFile(fileName);
        return;
    }
    if (formatId == QLatin1String("image")) {
        exportToImageFile(fileName);
        return;
    }

    const QString markdown = editor->toPlainText();
    const QString html = generateHtml(markdown);

    QByteArray data;
    if (!Exporter::generateText(formatId, markdown, html, data)) {
        QMessageBox::warning(this, tr("导出失败"),
                             tr("不支持的导出格式：%1").arg(fmt.displayName));
        return;
    }

    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly)) {
        QMessageBox::warning(this, tr("错误"),
                             tr("无法写入文件：%1").arg(file.errorString()));
        return;
    }
    const qint64 n = file.write(data);
    file.close();

    if (n != data.size()) {
        QMessageBox::warning(this, tr("错误"), tr("写入不完整，请检查磁盘空间。"));
        return;
    }

    statusBar()->showMessage(
        tr("已导出 %1（%2 KB）").arg(fmt.displayName).arg(data.size() / 1024), 3000);
}

/**
 * 导出 PDF 到指定路径。
 */
void MainWindow::exportToPdfFile(const QString &fileName)
{
    QTextEdit *editor = currentEditor();
    if (!editor) {
        return;
    }

    QPrinter printer(QPrinter::HighResolution);
    printer.setOutputFormat(QPrinter::PdfFormat);
    printer.setOutputFileName(fileName);
    printer.setPageSize(QPageSize(QPageSize::A4));
    printer.setPageMargins(QMarginsF(20, 20, 20, 20), QPageLayout::Millimeter);

    QTextDocument doc;
    doc.setHtml(generateHtml(editor->toPlainText()));
    doc.print(&printer);

    const QFileInfo fi(fileName);
    statusBar()->showMessage(
        tr("已导出 PDF（%1 KB）").arg(fi.size() / 1024), 3000);
}

/**
 * 导出 ODT（OpenDocument 文本）。
 *
 * 用 Qt 内建的 QTextDocumentWriter("ODF")，它能正确写出 odt 容器。
 */
void MainWindow::exportToOdtFile(const QString &fileName)
{
    QTextEdit *editor = currentEditor();
    if (!editor) {
        return;
    }

    QTextDocument doc;
    doc.setHtml(generateHtml(editor->toPlainText()));

    QTextDocumentWriter writer(fileName);
    writer.setFormat("ODF");
    if (!writer.write(&doc)) {
        QMessageBox::warning(this, tr("导出失败"),
                             tr("无法写入 ODT 文件。\n可能当前 Qt 未编译 ODF 写入插件。"));
        return;
    }

    const QFileInfo fi(fileName);
    statusBar()->showMessage(
        tr("已导出 OpenOffice 文档（%1 KB）").arg(fi.size() / 1024), 3000);
}

/**
 * 导出为图像（按扩展名决定 PNG / JPEG）。
 *
 * 做法：把预览用的 QTextDocument 以固定宽度排版，再渲染到 QImage。
 */
void MainWindow::exportToImageFile(const QString &fileName)
{
    QTextEdit *editor = currentEditor();
    if (!editor) {
        return;
    }

    // 正文宽度取 900px（接近 A4 在 96dpi 下的可视区）
    const int contentWidth = 900;
    const int margin = 40;
    const int totalWidth = contentWidth + margin * 2;

    QTextDocument doc;
    doc.setDefaultFont(QFont(QStringLiteral("Microsoft YaHei"), 11));
    doc.setHtml(generateHtml(editor->toPlainText()));
    doc.setTextWidth(contentWidth);

    const qreal docHeight = doc.size().height();
    // 高度上限保护：超长文档生成的图会极大，限制到 20000px
    const int maxHeight = 20000;
    const int imgHeight = qMin(static_cast<int>(docHeight) + margin * 2, maxHeight);

    QImage image(totalWidth, imgHeight, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::white);

    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);
    painter.translate(margin, margin);

    // 用 QTextDocument 的绘制接口，正确处理分页/裁剪
    QAbstractTextDocumentLayout::PaintContext ctx;
    ctx.clip = QRectF(0, 0, contentWidth, imgHeight - margin * 2);
    doc.documentLayout()->draw(&painter, ctx);
    painter.end();

    const QString suffix = QFileInfo(fileName).suffix().toLower();
    const char *fmt = (suffix == QLatin1String("jpg") || suffix == QLatin1String("jpeg"))
                          ? "JPEG" : "PNG";
    if (!image.save(fileName, fmt, 92)) {
        QMessageBox::warning(this, tr("导出失败"), tr("无法保存图像文件。"));
        return;
    }

    const QFileInfo fi(fileName);
    statusBar()->showMessage(
        tr("已导出图像 %1×%2（%3 KB）")
            .arg(totalWidth).arg(imgHeight).arg(fi.size() / 1024), 3000);
}
'''

s = s.rstrip() + '\n' + IMPL
io.open(p, 'w', encoding='utf-8', newline='\n').write(s)
print('  exportTo 等实现已追加')

# 补 include
need = ['#include <QPainter>', '#include <QAbstractTextDocumentLayout>',
        '#include <QPageSize>', '#include <QPageLayout>',
        '#include <QTextDocumentWriter>', '#include <QImage>']
adds = [n for n in need if n not in s]
if adds:
    s2 = io.open(p, encoding='utf-8').read()
    s2 = s2.replace('#include <QPrinter>',
                    '#include <QPrinter>\n' + '\n'.join(adds), 1)
    io.open(p, 'w', encoding='utf-8', newline='\n').write(s2)
    print('  include 补充 %d 个' % len(adds))
