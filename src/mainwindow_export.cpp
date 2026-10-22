/*
 * MainWindow —— 导出功能。
 *
 * 与编辑、视图逻辑分开：导出涉及文件对话框、打印子系统、
 * 图片渲染等一整套外部依赖，是最容易出兼容问题的地方。
 */

#include "mainwindow.h"
#include "appicon.h"
#include <QFile>
#include <QFileDialog>
#include <QMessageBox>
#include <QDir>
#include <QStandardPaths>
#include <QStringConverter>
#include <QPrinter>
#include <QPageSize>
#include <QPageLayout>
#include <QPainter>
#include <QImage>
#include <QTextStream>
#include <QTextDocumentWriter>

void MainWindow::exportToHtml()
{
    QPlainTextEdit *editor = currentEditor();
    if (!editor) {
        return;
    }

    QString fileName = QFileDialog::getSaveFileName(
        this,
        tr("导出为 HTML"),
        m_currentFolder,
        "HTML Files (*.html);;All Files (*)"
    );

    if (!fileName.isEmpty()) {
        QFile file(fileName);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QMessageBox::warning(this, tr("错误"), tr("无法保存文件: %1").arg(file.errorString()));
            return;
        }

        QString html = generateHtml(editor->toPlainText());
        
        QTextStream out(&file);
        out.setEncoding(QStringConverter::Utf8);
        out << html;
        out.flush();
        file.close();
        
        statusBar()->showMessage(tr("HTML 已导出"), 2000);
    }
}

void MainWindow::exportToPdf()
{
    QString fileName = QFileDialog::getSaveFileName(
        this,
        tr("导出为 PDF"),
        m_currentFolder,
        "PDF Files (*.pdf);;All Files (*)"
    );
    
    if (!fileName.isEmpty()) {
        QPrinter printer(QPrinter::HighResolution);
        printer.setOutputFormat(QPrinter::PdfFormat);
        printer.setOutputFileName(fileName);
        
        QTextDocument doc;
        doc.setHtml(generateHtml(currentEditor()->toPlainText()));
        doc.print(&printer);
        
        statusBar()->showMessage(tr("PDF 已导出"), 2000);
    }
}

void MainWindow::exportTo(const QString &formatId)
{
    QPlainTextEdit *editor = currentEditor();
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

void MainWindow::exportToPdfFile(const QString &fileName)
{
    QPlainTextEdit *editor = currentEditor();
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

void MainWindow::exportToOdtFile(const QString &fileName)
{
    QPlainTextEdit *editor = currentEditor();
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

void MainWindow::exportToImageFile(const QString &fileName)
{
    QPlainTextEdit *editor = currentEditor();
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
    const bool isJpeg = (suffix == QLatin1String("jpg") || suffix == QLatin1String("jpeg"));
    const char *fmt = isJpeg ? "JPEG" : "PNG";
    // 注意 QImage::save 的 quality 语义：
    //   - PNG 是无损格式，quality 直接映射到 zlib 压缩级别，
    //     0 = 最小文件（最高压缩），100 = 几乎不压缩。
    //     这里传 0，否则一张 900px 宽的图会膨胀到 5 MB。
    //   - JPEG 是有损格式，quality 才是真正的画质，用 92。
    const int quality = isJpeg ? 92 : 0;
    if (!image.save(fileName, fmt, quality)) {
        QMessageBox::warning(this, tr("导出失败"), tr("无法保存图像文件。"));
        return;
    }

    const QFileInfo fi(fileName);
    statusBar()->showMessage(
        tr("已导出图像 %1×%2（%3 KB）")
            .arg(totalWidth).arg(imgHeight).arg(fi.size() / 1024), 3000);
}

void MainWindow::showExportMenuForScreenshot()
{
    if (!m_exportMenu) {
        return;
    }
    // 找到「文件」菜单，用它作为父级定位
    QMenu *fileMenu = nullptr;
    for (QAction *a : menuBar()->actions()) {
        if (a->menu() && a->text().contains(tr("文件"))) {
            fileMenu = a->menu();
            break;
        }
    }
    if (!fileMenu) {
        return;
    }
    // 在文件菜单下方弹出导出子菜单
    const QPoint pos = menuBar()->mapToGlobal(QPoint(0, menuBar()->height()));
    m_exportMenu->popup(pos + QPoint(20, 30));
}
