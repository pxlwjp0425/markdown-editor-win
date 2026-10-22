#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
加 --bench-open 模式：
打开指定文件，等事件循环空闲后输出分段耗时，然后退出。

输出（写入报告文件）：
  phase|累计毫秒
  ...
  app_to_idle|xxx       进程启动 -> 首次空闲（用户可感知的启动完成）
"""
import io

pm = 'src/main.cpp'
m = io.open(pm, encoding='utf-8').read()

# ---- 选项 ----
if 'bench-open' not in m:
    old = '''    QCommandLineOption showExportMenuOption('''
    new = '''    // 性能基准：打开文件并输出分段耗时
    QCommandLineOption benchOption(
        QStringList() << "bench-open",
        "性能基准：打开指定文件，输出分段耗时后退出",
        "报告输出路径");
    parser.addOption(benchOption);

    QCommandLineOption showExportMenuOption('''
    assert old in m
    m = m.replace(old, new, 1)

    # ---- 处理逻辑：放在 show() 之前，需要 w ----
    old2 = '''    w.show();
    
    // 打开命令行指定的文件'''
    new2 = '''    w.show();

    // ---- 性能基准模式 ----
    if (parser.isSet(benchOption)) {
        const QString reportPath = parser.value(benchOption);
        const QStringList pos = parser.positionalArguments();
        if (pos.isEmpty()) {
            QFile f(reportPath);
            if (f.open(QIODevice::WriteOnly | QIODevice::Text)) {
                f.write("ERROR|no-input-file\\n");
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
            ts << log.join('\\n') << "\\n";
            ts.flush();
            f.close();
        }
        return 0;
    }

    // 打开命令行指定的文件'''
    assert old2 in m
    m = m.replace(old2, new2, 1)

    # include QElapsedTimer
    if '#include <QElapsedTimer>' not in m:
        m = m.replace('#include <QTextStream>', '#include <QElapsedTimer>\n#include <QTextStream>')

    io.open(pm, 'w', encoding='utf-8', newline='\n').write(m)
    print('  --bench-open 已加')
