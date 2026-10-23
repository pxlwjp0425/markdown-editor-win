/*
 * MainWindow —— 导出功能。
 *
 * 与编辑、视图逻辑分开：导出涉及文件对话框、打印子系统、
 * 图片渲染等一整套外部依赖，是最容易出兼容问题的地方。
 */

#include "mainwindow.h"
#include "appicon.h"
#include "pdfoutline.h"
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
#include <QTemporaryFile>
#include <QTextBlock>
#include <QTextFragment>
#include <QTextFormat>
#include <QAbstractTextDocumentLayout>
#include <QRegularExpression>
#include <QFont>
#include <QtMath>

/**
 * 导出用：如果正文没有手写 [TOC]，自动在开头插入一个目录，
 * 让导出的 PDF / HTML 带可点击的章节导航（标题锚点由渲染器统一生成）。
 */
static QString withAutoToc(const QString &markdown)
{
    if (markdown.contains(QLatin1String("[TOC]"), Qt::CaseInsensitive)) {
        return markdown;
    }
    return QStringLiteral("[TOC]\n\n") + markdown;
}

// ---- PDF 侧边栏书签支持 ----

// 解析 markdown 标题，返回 (层级, 原始文本)
static QVector<QPair<int, QString>> extractMdHeadings(const QString &md)
{
    QVector<QPair<int, QString>> res;
    const QStringList lines = md.split(QLatin1Char('\n'));
    for (const QString &line : lines) {
        if (line.startsWith(QLatin1String("```"))) {
            continue;  // 跳过围栏代码块
        }
        int h = 0;
        while (h < line.size() && line.at(h) == QLatin1Char('#')) {
            ++h;
        }
        if (h >= 1 && h <= 6) {
            QString t = line.mid(h).trimmed();
            while (t.endsWith(QLatin1Char('#')) && !t.isEmpty()) {
                t.chop(1);
            }
            t = t.trimmed();
            if (!t.isEmpty()) {
                res.append(qMakePair(h, t));
            }
        }
    }
    return res;
}

// 去掉常见内联 markdown 标记，便于与渲染后的纯文本精确匹配
static QString stripInlineMd(const QString &s)
{
    QString t = s;
    t.replace(QRegularExpression("\\[([^\\]]*)\\]\\([^)]*\\)"), QStringLiteral("\\1"));
    t.remove(QRegularExpression("(\\*\\*|__|\\*|`|_)"));
    return t.trimmed();
}

QByteArray MainWindow::renderPdfBytes(const QString &markdown)
{
    QPrinter printer(QPrinter::HighResolution);
    printer.setOutputFormat(QPrinter::PdfFormat);
    printer.setPageSize(QPageSize(QPageSize::A4));
    printer.setPageMargins(QMarginsF(20, 20, 20, 20), QPageLayout::Millimeter);

    // QPrinter 的 PdfFormat 不直接接受 QBuffer，先输出到临时文件再读回字节。
    QTemporaryFile tmp;
    if (!tmp.open()) {
        return QByteArray();
    }
    const QString tmpPath = tmp.fileName();
    tmp.close();
    printer.setOutputFileName(tmpPath);

    const QString html = generateHtml(withAutoToc(markdown));
    QTextDocument doc;
    doc.setHtml(html);
    // 用页面尺寸（点）布局，使文档坐标与 PDF 点一致（宽度决定换行/总高度）。
    const QSizeF pageSize = printer.pageRect(QPrinter::Point).size();
    doc.setPageSize(pageSize);

    // 在打印之前采集：文档总高度 + 每个标题的纵向位置。
    // 关键：打印后文档布局会被重置/塌缩，必须提前取坐标。
    const double docTotalH = doc.size().height();

    const auto headings = extractMdHeadings(markdown);
    QVector<QPair<QString, double>> headingY;  // (干净标题, 文档内纵坐标)
    if (!headings.isEmpty()) {
        const QAbstractTextDocumentLayout *layout = doc.documentLayout();
        // 真正的标题块用加粗字体（<hN> 默认 bold，weight≈700），
        // 而自动 [TOC] 目录项（<a href> 里）和正文都是常规字重（≈400）。
        // 用字重区分，避免匹配到顶部目录项（其文本与标题相同、都挤在顶部）。
        auto blockIsHeading = [](const QTextBlock &b) {
            for (auto it = b.begin(); !it.atEnd(); ++it) {
                if (it.fragment().charFormat().fontWeight() >= QFont::DemiBold) {
                    return true;
                }
            }
            return false;
        };

        int hi = 0;
        QTextBlock block = doc.firstBlock();
        while (block.isValid() && hi < headings.size()) {
            if (block.textList() || !blockIsHeading(block)) {
                block = block.next();
                continue;
            }
            const QString blockText = block.text().trimmed();
            if (!blockText.isEmpty()) {
                const QString want = stripInlineMd(headings.at(hi).second);
                const bool match = !want.isEmpty()
                        && (blockText == want
                            || (want.length() > 3 && blockText.contains(want)));
                if (match) {
                    headingY.append(qMakePair(want, layout->blockBoundingRect(block).top()));
                    ++hi;
                }
            }
            block = block.next();
        }
    }

    // 打印到临时文件
    doc.print(&printer);

    QFile f(tmpPath);
    if (!f.open(QIODevice::ReadOnly)) {
        return QByteArray();
    }
    QByteArray pdf = f.readAll();
    f.close();
    QFile::remove(tmpPath);
    if (pdf.isEmpty()) {
        return pdf;
    }

    // 页数（以真实 PDF 为准，避免分页取整误差）
    const QVector<int> pageObjNums = pdfPageObjectNumbers(pdf);
    const int N = pageObjNums.size();
    if (N <= 0) {
        return pdf;  // 解析失败则退回无书签 PDF
    }

    // 用「相对纵向比例 × 页数」定位：与 DPI / 设备像素无关。
    // 每个标题的全局纵向比例 frac ∈ [0,1)，乘以页数得到页位置，整数部分为页码，
    // 小数部分为页内比例，再乘以可打印页高（点）即为页内纵坐标。
    const double paperHpt = printer.paperRect(QPrinter::Point).height();
    const double topMgnPt = printer.pageLayout().margins(QPageLayout::Point).top();
    const double pageRectHpt = pageSize.height();

    QVector<PdfOutlineEntry> entries;
    entries.reserve(headingY.size());
    for (const auto &hy : headingY) {
        const double frac = (docTotalH > 0.0) ? (hy.second / docTotalH) : 0.0;
        const double pos = frac * N;
        const int pg = qBound(0, qFloor(pos), N - 1);
        const double withinFrac = qBound(0.0, pos - pg, 1.0);
        PdfOutlineEntry e;
        e.title = hy.first;
        e.pageIndex = pg;
        e.y = paperHpt - topMgnPt - withinFrac * pageRectHpt;
        entries.append(e);
    }

    return injectPdfOutline(pdf, entries, N);
}

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
        const QByteArray bytes = renderPdfBytes(currentEditor()->toPlainText());
        QFile file(fileName);
        if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size()) {
            QMessageBox::warning(this, tr("错误"),
                                 tr("写入 PDF 失败：%1").arg(file.errorString()));
            return;
        }
        file.close();
        statusBar()->showMessage(tr("PDF 已导出（含侧边栏书签）"), 2000);
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

    const QByteArray bytes = renderPdfBytes(editor->toPlainText());
    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size()) {
        QMessageBox::warning(this, tr("错误"),
                             tr("写入 PDF 失败：%1").arg(file.errorString()));
        return;
    }
    file.close();

    const QFileInfo fi(fileName);
    statusBar()->showMessage(
        tr("已导出 PDF（%1 KB，含侧边栏书签）").arg(fi.size() / 1024), 3000);
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
