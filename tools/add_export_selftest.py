#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
给程序加 --self-test-export 钩子：把所有导出格式写到指定目录。

用法：MarkdownEditor.exe --self-test-export <输出目录> [源文件.md]
"""
import io

p = 'src/main.cpp'
s = io.open(p, encoding='utf-8').read()

# ---- 选项注册 ----
if 'self-test-export' not in s:
    old = '''    QCommandLineOption probeOption(QStringLiteral("probe-export"), QStringLiteral("probe"));'''
    new = '''    // 自检：把所有导出格式写到指定目录
    QCommandLineOption selfTestExportOption(
        QStringList() << "self-test-export",
        "自检：把所有导出格式写到指定目录",
        "输出目录");
    parser.addOption(selfTestExportOption);

    QCommandLineOption probeOption(QStringLiteral("probe-export"), QStringLiteral("probe"));'''
    assert old in s
    s = s.replace(old, new, 1)

    # ---- 处理逻辑（插在 GUI 模式之前，需要 MainWindow 实例） ----
    old2 = '''    // ---- 导出能力探测（临时） ----'''
    new2 = '''    // ---- 自检：批量导出 ----
    if (parser.isSet(selfTestExportOption)) {
        w.show();
        QCoreApplication::processEvents();
        const QString outDir = parser.value(selfTestExportOption);
        QDir().mkpath(outDir);
        const QStringList lines = w.selfTestExportAll(outDir);
        QFile f(QDir(outDir).filePath(QStringLiteral("_report.txt")));
        if (f.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream ts(&f);
            ts.setEncoding(QStringConverter::Utf8);
            ts << lines.join('\\n') << "\\n";
            ts.flush();
            f.close();
        }
        return 0;
    }

    // ---- 导出能力探测（临时） ----'''
    assert old2 in s
    s = s.replace(old2, new2, 1)
    io.open(p, 'w', encoding='utf-8', newline='\n').write(s)
    print('  main.cpp 已加 --self-test-export')

# ---- mainwindow.h 声明 ----
ph = 'src/mainwindow.h'
h = io.open(ph, encoding='utf-8').read()
if 'selfTestExportAll' not in h:
    h = h.replace('    QString selfTestTyporaUi();',
                  '    QString selfTestTyporaUi();\n\n'
                  '    // 自检：把所有导出格式写到指定目录，返回 KEY=VALUE 报告\n'
                  '    QStringList selfTestExportAll(const QString &outDir);')
    io.open(ph, 'w', encoding='utf-8', newline='\n').write(h)
    print('  mainwindow.h 已声明')

# ---- mainwindow.cpp 实现 ----
pc = 'src/mainwindow.cpp'
c = io.open(pc, encoding='utf-8').read()

if 'selfTestExportAll' not in c:
    IMPL = r'''

/**
 * 自检：把所有导出格式写到指定目录。
 *
 * 输出格式：每行 "格式id|文件路径|字节数|成功?"
 * 供 tools/selftest_export.py 用 zipfile / xml 解析器做真实性校验。
 */
QStringList MainWindow::selfTestExportAll(const QString &outDir)
{
    QStringList report;

    QTextEdit *editor = currentEditor();
    if (!editor) {
        report << QStringLiteral("ERROR|no-editor");
        return report;
    }

    const QString markdown = editor->toPlainText();
    const QString html = generateHtml(markdown);

    // 统计源文档规模，便于对照
    report << QStringLiteral("SOURCE|chars=%1|lines=%2")
                  .arg(markdown.length())
                  .arg(markdown.count(QLatin1Char('\n')) + 1);

    for (const Exporter::Format &f : Exporter::formats()) {
        // pdf / odt / image 需要额外上下文，单独走
        if (f.id == QLatin1String("pdf") || f.id == QLatin1String("odt") ||
            f.id == QLatin1String("image")) {
            continue;
        }
        QByteArray data;
        if (!Exporter::generateText(f.id, markdown, html, data)) {
            report << QStringLiteral("%1|-|0|FAIL(no-generator)").arg(f.id);
            continue;
        }
        const QString path = QDir(outDir).filePath(
            QStringLiteral("export-") + f.id + QLatin1Char('.') + f.defaultExt);
        QFile file(path);
        if (!file.open(QIODevice::WriteOnly)) {
            report << QStringLiteral("%1|-|0|FAIL(cannot-write)").arg(f.id);
            continue;
        }
        const qint64 n = file.write(data);
        file.close();
        report << QStringLiteral("%1|%2|%3|%4")
                      .arg(f.id, path)
                      .arg(n)
                      .arg(n == data.size() ? QStringLiteral("OK")
                                            : QStringLiteral("FAIL(short-write)"));
    }

    // ---- PDF ----
    {
        const QString path = QDir(outDir).filePath(QStringLiteral("export-pdf.pdf"));
        exportToPdfFile(path);
        const QFileInfo fi(path);
        report << QStringLiteral("pdf|%1|%2|%3")
                      .arg(path).arg(fi.size())
                      .arg(fi.size() > 0 ? QStringLiteral("OK") : QStringLiteral("FAIL"));
    }

    // ---- ODT ----
    {
        const QString path = QDir(outDir).filePath(QStringLiteral("export-odt.odt"));
        QTextDocument doc;
        doc.setHtml(html);
        QTextDocumentWriter writer(path);
        writer.setFormat("ODF");
        const bool ok = writer.write(&doc);
        const QFileInfo fi(path);
        report << QStringLiteral("odt|%1|%2|%3")
                      .arg(path).arg(fi.size())
                      .arg(ok && fi.size() > 0 ? QStringLiteral("OK") : QStringLiteral("FAIL"));
    }

    // ---- 图片 ----
    {
        const QString path = QDir(outDir).filePath(QStringLiteral("export-image.png"));
        exportToImageFile(path);
        const QFileInfo fi(path);
        report << QStringLiteral("image|%1|%2|%3")
                      .arg(path).arg(fi.size())
                      .arg(fi.size() > 0 ? QStringLiteral("OK") : QStringLiteral("FAIL"));
    }

    return report;
}
'''
    c = c.rstrip() + '\n' + IMPL
    io.open(pc, 'w', encoding='utf-8', newline='\n').write(c)
    print('  mainwindow.cpp 已加 selfTestExportAll')
