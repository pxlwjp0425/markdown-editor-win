#include "mainwindow.h"
#include "appicon.h"

#include <QApplication>
#include <QIcon>
#include <QSvgRenderer>
#include <QBuffer>
#include <QPainter>
#include <QImage>
#include <QStyleFactory>
#include <QFileInfo>
#include <QCommandLineParser>
#include <QElapsedTimer>
#include <QTextStream>
#include <QTextDocumentWriter>
#include <QFile>
#include <QStringConverter>
#include <QPrinter>
#include <QTextDocument>

/**
 * 把内置 SVG 渲染成多尺寸 PNG 并拼成 ICO。
 *
 * ICO 结构：
 *   ICONDIR      : reserved(2) type(2) count(2)
 *   ICONDIRENTRY : width(1) height(1) colors(1) reserved(1)
 *                  planes(2) bitcount(2) bytes(4) offset(4)
 *   data         : 每张图的 PNG 字节流
 * width/height 字段只有 1 字节，0 表示 256。
 */
static bool exportAppIcon(const QString &outPath, QString *err)
{
    struct Spec {
        int size;
        const char *res;
    };
    // 小尺寸用简化版（只有 M），大尺寸用完整版（M + 向下箭头）：
    // 完整版的 "M↓" 在 16/24/32 会糊成一团白点。
    static const Spec specs[] = {
        {16,  ":/icons/app-icon-small.svg"},
        {24,  ":/icons/app-icon-small.svg"},
        {32,  ":/icons/app-icon-small.svg"},
        {48,  ":/icons/app-icon.svg"},
        {64,  ":/icons/app-icon.svg"},
        {128, ":/icons/app-icon.svg"},
        {256, ":/icons/app-icon.svg"},
    };
    const int count = int(sizeof(specs) / sizeof(specs[0]));

    QList<QByteArray> pngs;
    for (int i = 0; i < count; ++i) {
        QSvgRenderer renderer(QString::fromLatin1(specs[i].res));
        if (!renderer.isValid()) {
            *err = QStringLiteral("无法加载 SVG: %1").arg(QString::fromLatin1(specs[i].res));
            return false;
        }
        QImage img(specs[i].size, specs[i].size, QImage::Format_ARGB32_Premultiplied);
        img.fill(Qt::transparent);
        QPainter painter(&img);
        painter.setRenderHint(QPainter::Antialiasing, true);
        painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
        renderer.render(&painter);
        painter.end();

        QByteArray png;
        QBuffer buffer(&png);
        buffer.open(QIODevice::WriteOnly);
        if (!img.save(&buffer, "PNG")) {
            *err = QStringLiteral("PNG 编码失败（size=%1）").arg(specs[i].size);
            return false;
        }
        buffer.close();
        pngs.append(png);
    }

    QByteArray ico;
    auto putU16 = [&ico](quint16 v) {
        ico.append(char(v & 0xFF));
        ico.append(char((v >> 8) & 0xFF));
    };
    auto putU32 = [&ico](quint32 v) {
        ico.append(char(v & 0xFF));
        ico.append(char((v >> 8) & 0xFF));
        ico.append(char((v >> 16) & 0xFF));
        ico.append(char((v >> 24) & 0xFF));
    };

    putU16(0);
    putU16(1);
    putU16(static_cast<quint16>(count));

    quint32 offset = 6 + 16 * static_cast<quint32>(count);
    for (int i = 0; i < count; ++i) {
        const int s = specs[i].size;
        ico.append(char(s >= 256 ? 0 : s));
        ico.append(char(s >= 256 ? 0 : s));
        ico.append(char(0));
        ico.append(char(0));
        putU16(1);
        putU16(32);
        putU32(static_cast<quint32>(pngs.at(i).size()));
        putU32(offset);
        offset += static_cast<quint32>(pngs.at(i).size());
    }
    for (const QByteArray &png : pngs) {
        ico.append(png);
    }

    QFile file(outPath);
    if (!file.open(QIODevice::WriteOnly)) {
        *err = QStringLiteral("无法写入: %1").arg(file.errorString());
        return false;
    }
    const qint64 written = file.write(ico);
    file.close();
    if (written != ico.size()) {
        *err = QStringLiteral("写入不完整");
        return false;
    }
    return true;
}

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("Markdown Editor");
    app.setApplicationDisplayName("Markdown Editor");
    app.setApplicationVersion("1.0.0");
    app.setOrganizationName("MarkdownEditor");
    
    // 设置应用程序风格
    app.setStyle(QStyleFactory::create("Fusion"));
    
    // 设置默认字体
    // Windows 上优先用 Segoe UI，中文回退到微软雅黑
    QFont defaultFont("Segoe UI", 10);
    defaultFont.setStyleStrategy(QFont::PreferAntialias);
    app.setFont(defaultFont);
    
    // 应用图标（内置 SVG，同时用于任务栏与窗口标题栏）
    const QIcon appIcon = makeAppIcon();
    if (!appIcon.isNull()) {
        app.setWindowIcon(appIcon);
    }
    
    // 命令行：支持拖拽/双击 md 文件时由 shell 传入路径
    QCommandLineParser parser;
    parser.setApplicationDescription("Markdown Editor - 简洁高效的 Markdown 编辑器");
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addPositionalArgument("file", "要打开的 Markdown 文件");
    
    QCommandLineOption darkOption(
        QStringList() << "dark",
        "启动时启用深色模式");
    parser.addOption(darkOption);
    
    // 单独预览模式（只显示预览区）
    QCommandLineOption previewOnlyOption(
        QStringList() << "preview-only",
        "启动时进入单独预览模式（只显示预览区，隐藏编辑器与侧边栏）");
    parser.addOption(previewOnlyOption);

    // 自检：验证「单独预览模式」菜单项的连接与状态切换
    QCommandLineOption selfTestOption(
        QStringList() << "self-test-preview-only",
        "自检：触发「单独预览模式」菜单项并输出结果");
    parser.addOption(selfTestOption);

    // 自检：把所有导出格式写到指定目录
    QCommandLineOption selfTestExportOption(
        QStringList() << "self-test-export",
        "自检：把所有导出格式写到指定目录",
        "输出目录");
    parser.addOption(selfTestExportOption);

    // 性能基准：打开文件并输出分段耗时
    QCommandLineOption benchOption(
        QStringList() << "bench-open",
        "性能基准：打开指定文件，输出分段耗时后退出",
        "报告输出路径");
    parser.addOption(benchOption);

    // 生成应用图标（多尺寸 ICO）
    QCommandLineOption exportIconOption(
        QStringList() << "export-icon",
        "把内置应用图标渲染为多尺寸 ICO 文件",
        "输出路径");
    parser.addOption(exportIconOption);

    QCommandLineOption showExportMenuOption(
        QStringList() << "show-export-menu",
        "截图辅助：启动后弹出「文件 > 导出」子菜单");
    parser.addOption(showExportMenuOption);

    QCommandLineOption probeOption(QStringLiteral("probe-export"), QStringLiteral("probe"));
    parser.addOption(probeOption);

    // 自检：Typora UI 规格
    QCommandLineOption selfTestUiOption(
        QStringList() << "self-test-typora-ui",
        "自检：输出 Typora UI 规格（菜单/快捷键/格式与块级引擎）的验证结果");
    parser.addOption(selfTestUiOption);

    // 无界面导出模式：用于自动化测试与脚本化转换
    QCommandLineOption exportOption(
        QStringList() << "export-html",
        "无界面模式：将输入文件转换为 HTML 并写入指定路径",
        "输出路径");
    parser.addOption(exportOption);

    QCommandLineOption exportPdfOption(
        QStringList() << "export-pdf",
        "无界面模式：将输入文件转换为 PDF 并写入指定路径",
        "输出路径");
    parser.addOption(exportPdfOption);

    // 主题选择（用于无界面导出时指定配色）
    QCommandLineOption themeOption(
        QStringList() << "theme",
        "指定主题：default / github / github-dark / night / newsprint / pixyll / whitey",
        "主题 id");
    parser.addOption(themeOption);

    // 调试：导出文档大纲（标题层级与行号）
    QCommandLineOption dumpOutlineOption(
        QStringList() << "dump-outline",
        "输出文档大纲（标题层级与行号）到标准输出");
    parser.addOption(dumpOutlineOption);


    parser.process(app);

    // ---- 无界面导出模式 ----
    if (parser.isSet(exportOption)) {
        const QStringList pos = parser.positionalArguments();
        if (pos.isEmpty()) {
            qWarning("--export-html 需要指定输入文件");
            return 2;
        }
        QFile inFile(pos.first());
        if (!inFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
            qWarning("无法读取输入文件: %s", qPrintable(pos.first()));
            return 3;
        }
        QTextStream in(&inFile);
        in.setEncoding(QStringConverter::Utf8);
        const QString markdown = in.readAll();
        inFile.close();
        
        // 复用主窗口的渲染逻辑，保证与 GUI 输出完全一致
        MainWindow w;
        // 无界面模式：必须最先设置，避免后续 UI 副作用触发事件循环依赖
        w.setHeadlessMode(true);
        if (parser.isSet(themeOption)) {
            w.applyThemeTemporarily(parser.value(themeOption));
        }
        const QString html = w.renderMarkdownToHtml(markdown);
        
        QFile outFile(parser.value(exportOption));
        if (!outFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
            qWarning("无法写入输出文件: %s", qPrintable(parser.value(exportOption)));
            return 4;
        }
        QTextStream out(&outFile);
        out.setEncoding(QStringConverter::Utf8);
        out << html;
        out.flush();
        outFile.close();
        return 0;
    }

    // ---- 无界面 PDF 导出模式 ----
    if (parser.isSet(exportPdfOption)) {
        const QStringList pos = parser.positionalArguments();
        if (pos.isEmpty()) {
            qWarning("--export-pdf 需要指定输入文件");
            return 2;
        }
        QFile inFile(pos.first());
        if (!inFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
            qWarning("无法读取输入文件: %s", qPrintable(pos.first()));
            return 3;
        }
        QTextStream in(&inFile);
        in.setEncoding(QStringConverter::Utf8);
        const QString markdown = in.readAll();
        inFile.close();

        MainWindow w;
        w.setHeadlessMode(true);
        if (parser.isSet(themeOption)) {
            w.applyThemeTemporarily(parser.value(themeOption));
        }
        const QString html = w.renderMarkdownToHtml(markdown);

        // QPrinter 的 PdfFormat 在 Windows 上是纯软件输出，不依赖已安装的打印机
        QPrinter printer(QPrinter::HighResolution);
        printer.setOutputFormat(QPrinter::PdfFormat);
        printer.setOutputFileName(parser.value(exportPdfOption));

        QTextDocument doc;
        doc.setHtml(html);
        doc.print(&printer);
        return 0;
    }
    


    // ---- 导出能力探测（临时） ----
    if (parser.isSet(QStringLiteral("probe-export"))) {
        QFile probe(QStringLiteral("probe-export.txt"));
        if (!probe.open(QIODevice::WriteOnly | QIODevice::Text)) {
            qWarning("无法写入探测文件: %s", qPrintable(probe.errorString()));
            return 1;
        }
        QTextStream s(&probe);
        s.setEncoding(QStringConverter::Utf8);
        s << "QTextDocumentWriter formats://n";
        for (const QByteArray &f : QTextDocumentWriter::supportedDocumentFormats()) {
            s << "  [" << QString::fromLatin1(f) << "]\n";
        }
        s.flush();
        probe.close();
        return 0;
    }

    // ---- 正常 GUI 模式 ----
    MainWindow w;

    // ---- 生成应用图标 ----
    if (parser.isSet(exportIconOption)) {
        QString err;
        const QString out = parser.value(exportIconOption);
        const bool ok = exportAppIcon(out, &err);
        QFile log(out + QStringLiteral(".log"));
        if (log.open(QIODevice::WriteOnly | QIODevice::Text)) {
            log.write(ok ? QByteArray("OK\n") : QByteArray("FAIL: ") + err.toUtf8() + '\n');
            log.close();
        }
        return ok ? 0 : 1;
    }

    // ---- 自检：批量导出 ----
    if (parser.isSet(selfTestExportOption)) {
        // 先打开位置参数指定的文件 —— 否则导出的是空文档
        const QStringList posArgs = parser.positionalArguments();
        if (!posArgs.isEmpty()) {
            w.openFileAtStartup(posArgs.first());
            QCoreApplication::processEvents();
        }
        w.show();
        QCoreApplication::processEvents();
        const QString outDir = parser.value(selfTestExportOption);
        QDir().mkpath(outDir);
        const QStringList lines = w.selfTestExportAll(outDir);
        QFile f(QDir(outDir).filePath(QStringLiteral("_report.txt")));
        if (f.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream ts(&f);
            ts.setEncoding(QStringConverter::Utf8);
            ts << lines.join('\n') << "\n";
            ts.flush();
            f.close();
        }
        return 0;
    }

    // 自检模式：Typora UI 规格
    if (parser.isSet(selfTestUiOption)) {
        // 显式用 UTF-8 输出，避免中文在控制台乱码
        QTextStream sout(stdout);
        sout.setEncoding(QStringConverter::Utf8);
        sout << w.selfTestTyporaUi() << "\n";
        sout.flush();
        return 0;
    }

    // 自检模式：验证「单独预览模式」菜单项
    if (parser.isSet(selfTestOption)) {
        w.show();
        QCoreApplication::processEvents();
        QTextStream sout(stdout);
        sout.setEncoding(QStringConverter::Utf8);
        sout << w.selfTestPreviewOnlyToggle() << "\n";
        sout.flush();
        return 0;
    }
    
    // 主题（GUI 模式下也生效，便于截图/演示时指定配色）
    // 用临时版本：命令行指定的配色不该覆盖用户保存的主题偏好。
    if (parser.isSet(themeOption)) {
        w.applyThemeTemporarily(parser.value(themeOption));
    }
    
    // 深色模式需在窗口创建后设置（高亮器随标签页创建）
    if (parser.isSet(darkOption)) {
        w.setDarkModeAtStartup(true);
    }
    
    // 单独预览模式（演示 / 截图 / 全屏阅读）
    if (parser.isSet(previewOnlyOption)) {
        w.setPreviewOnlyModeAtStartup(true);
    }
    
    w.show();

    // ---- 性能基准模式 ----
    if (parser.isSet(benchOption)) {
        const QString reportPath = parser.value(benchOption);
        const QStringList pos = parser.positionalArguments();
        if (pos.isEmpty()) {
            QFile f(reportPath);
            if (f.open(QIODevice::WriteOnly | QIODevice::Text)) {
                f.write("ERROR|no-input-file\n");
            }
            return 1;
        }
        const QString target = pos.first();

        w.setBenchEnabled(true);
        // 先等窗口真正显示出来，避免把首帧绘制算进打开耗时
        QCoreApplication::processEvents();

        QElapsedTimer wall;
        wall.start();

        w.openFileAtStartup(target);

        // 等到事件循环空闲（首帧渲染完成）
        QCoreApplication::processEvents();
        QCoreApplication::processEvents();

        const double wallMs = wall.nsecsElapsed() / 1000000.0;

        QStringList log = w.benchLog();
        log << QStringLiteral("wall_open_ms|%1").arg(wallMs, 0, 'f', 2);

        QFile f(reportPath);
        if (f.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream ts(&f);
            ts.setEncoding(QStringConverter::Utf8);
            ts << log.join('\n') << "\n";
            ts.flush();
            f.close();
        }
        return 0;
    }

    // 打开命令行指定的文件
    const QStringList args = parser.positionalArguments();
    if (!args.isEmpty()) {
        QFileInfo info(args.first());
        if (info.exists() && info.isFile()) {
            w.openFileAtStartup(info.absoluteFilePath());
        }
    }

    // 截图辅助：稍等窗口稳定后弹出「文件 > 导出」子菜单
    if (parser.isSet(showExportMenuOption)) {
        QTimer::singleShot(1200, &w, [&w]() { w.showExportMenuForScreenshot(); });
    }
    
    return app.exec();
}
