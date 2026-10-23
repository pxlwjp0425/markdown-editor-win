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
#include <QStandardPaths>
#include <QDir>
#include <QDateTime>
#include <QMessageBox>

// 宽字符命令行 API（GetCommandLineW / CommandLineToArgvW）
#include <windows.h>

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

/**
 * 启动参数诊断。
 *
 * 「双击 .md 后编辑器是空的」这类问题，最难定位的就是
 * shell 到底把什么传进来了 —— 双击、命令行、拖拽、脚本启动
 * 四条路径的传参方式各不相同，光看代码猜不出来。
 *
 * 这里在「带了参数启动」时把收到的原始参数与当前工作目录
 * 追加写到 %TEMP%\MarkdownEditor-startup.log，
 * 不带参数启动（最常见的情况）则什么都不写。
 *
 * 日志只追加、不清理，多次双击会形成时间线，便于对比。
 */
static void logStartupArgs(int argc, char *argv[])
{
    if (argc <= 1) {
        return;
    }

    const QString dir = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    if (dir.isEmpty()) {
        return;
    }

    QFile f(QDir(dir).filePath(QStringLiteral("MarkdownEditor-startup.log")));
    if (!f.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        return;
    }

    QTextStream out(&f);
    out.setEncoding(QStringConverter::Utf8);
    out << "---- " << QDateTime::currentDateTime().toString(Qt::ISODate) << "\n";
    out << "argc=" << argc << "\n";
    for (int i = 0; i < argc; ++i) {
        out << "  argv[" << i << "]=" << QString::fromLocal8Bit(argv[i]) << "\n";
    }
    out << "cwd=" << QDir::currentPath() << "\n";
    out.flush();
    f.close();
}

/**
 * 取「宽字符版」的命令行参数。
 *
 * Windows GUI 程序的 main(argc, argv) 收到的是窄字符 argv，
 * 由 CRT 按本地 ANSI 代码页转换而来（中文 Windows 上是 GBK）——
 * emoji、日文假名、部分符号无法用 GBK 表示，会被替换成 '?'。
 *
 * 实测：双击 D:\User\obsidian\📚教材\...\卷6-合并.md 时，
 * argv[1] 变成 "D:\User\obsidian\??教材\..."（📚 是代理对，
 * 占两个 UTF-16 码元，于是变成两个 '?'），QFileInfo::exists()
 * 自然为假，用户看到的就是「无法打开文件」。
 * 而 QDir::currentPath() 是对的 —— 它走的是 GetCurrentDirectoryW()。
 *
 * 所以必须自己用 GetCommandLineW() + CommandLineToArgvW()
 * 取原始宽字符命令行，绕开 CRT 的代码页转换。
 */
static QStringList wideCommandLineArgs()
{
    int wideArgc = 0;
    LPWSTR *wideArgv = CommandLineToArgvW(GetCommandLineW(), &wideArgc);
    if (!wideArgv) {
        return QStringList();
    }

    QStringList args;
    args.reserve(wideArgc);
    for (int i = 0; i < wideArgc; ++i) {
        args << QString::fromWCharArray(wideArgv[i]);
    }
    LocalFree(wideArgv);
    return args;
}

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    logStartupArgs(argc, argv);
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

    // 自检：合并 Markdown 端到端
    QCommandLineOption selfTestMergeOption(
        QStringList() << "self-test-merge",
        "自检：端到端验证合并 Markdown（对话框 -> 合并 -> 新标签）");
    parser.addOption(selfTestMergeOption);

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


    // 命令行解析必须用宽字符版本。
    //
    // parser.process(app) 内部用的是 QCoreApplication::arguments()，
    // 而后者是 QString::fromLocal8Bit(argv[i]) —— 走本地代码页（GBK），
    // 含 emoji 的路径会被破坏。所以这里改为用 wideCommandLineArgs()
    // 自己解析，并手动补上 process() 原本会做的
    // 「解析失败即退出」与 --version / --help 处理。
    QStringList cmdArgs = wideCommandLineArgs();
    if (cmdArgs.isEmpty()) {
        // 宽字符取不到时退回 Qt 自己的参数（至少不会崩）
        cmdArgs = QCoreApplication::arguments();
    }
    if (!parser.parse(cmdArgs)) {
        fprintf(stderr, "%s\n", qPrintable(parser.errorText()));
        return EXIT_FAILURE;
    }
    if (parser.isSet(QStringLiteral("version"))) {
        parser.showVersion();
    }
    if (parser.isSet(QStringLiteral("help"))) {
        parser.showHelp();
    }

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

        // 复用与 GUI 一致的渲染（含 PDF 侧边栏书签注入）
        const QByteArray pdf = w.renderPdfBytes(markdown);
        QFile out(parser.value(exportPdfOption));
        if (!out.open(QIODevice::WriteOnly) || out.write(pdf) != pdf.size()) {
            qWarning("无法写入 PDF: %s", qPrintable(parser.value(exportPdfOption)));
            return 4;
        }
        out.close();
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

    // 自检模式：合并 Markdown 端到端
    if (parser.isSet(selfTestMergeOption)) {
        QTextStream sout(stdout);
        sout.setEncoding(QStringConverter::Utf8);
        sout << w.selfTestMerge() << "\n";
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
    //
    // 双击 .md 走的就是这条路。这里原来是「静默」的：
    // 参数没解析出文件、或文件不存在时，什么都不说，
    // 用户看到的就是一个空的「未命名」编辑器，没有任何线索。
    //
    // 现在把相对路径也按当前工作目录兜底解析，
    // 并在打不开时给出明确提示。
    const QStringList args = parser.positionalArguments();
    if (!args.isEmpty()) {
        const QString rawPath = args.first();
        QFileInfo info(rawPath);
        // shell 传来的必然是绝对路径；相对路径按 CWD 解析，
        // 覆盖从终端用相对路径启动的情况。
        if (!info.isAbsolute() && !info.exists()) {
            const QString resolved = QDir::currentPath() + QLatin1Char('/') + rawPath;
            if (QFileInfo::exists(resolved)) {
                info = QFileInfo(resolved);
            }
        }
        if (info.exists() && info.isFile()) {
            w.openFileAtStartup(info.absoluteFilePath());
        } else {
            qWarning("无法打开启动参数指定的文件: %s (cwd=%s)",
                     qPrintable(rawPath), qPrintable(QDir::currentPath()));
            QMessageBox::warning(&w, QObject::tr("无法打开文件"),
                                 QObject::tr("无法打开文件：\n%1").arg(rawPath));
        }
    }

    // 截图辅助：稍等窗口稳定后弹出「文件 > 导出」子菜单
    if (parser.isSet(showExportMenuOption)) {
        QTimer::singleShot(1200, &w, [&w]() { w.showExportMenuForScreenshot(); });
    }
    
    return app.exec();
}
