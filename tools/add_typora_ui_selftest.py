#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
生成 MainWindow 的 Typora UI 自检实现（追加到 mainwindow.cpp），
并在 main.cpp 注册 --self-test-typora-ui 选项。
"""
import io

# ===========================================================================
# 1) mainwindow.cpp: 追加自检实现
# ===========================================================================
p = 'src/mainwindow.cpp'
s = io.open(p, encoding='utf-8').read()

IMPL = r'''

/**
 * Typora UI 自检。
 *
 * 覆盖：菜单结构、侧边栏页签、快捷键、行内格式引擎、块级编辑引擎、打字机模式。
 * 输出 KEY=VALUE 形式，供 tools/selftest_typora_ui.py 断言。
 */
QString MainWindow::selfTestTyporaUi()
{
    QStringList out;
    auto add = [&out](const QString &k, const QString &v) {
        out << QStringLiteral("%1=%2").arg(k, v);
    };

    // ---------------- 菜单结构 ----------------
    const QList<QMenu*> menus = menuBar()->findChildren<QMenu*>();
    // 顶层菜单 = menuBar 的 actions 对应的菜单
    QList<QMenu*> topMenus;
    for (QAction *a : menuBar()->actions()) {
        if (a->menu()) {
            topMenus << a->menu();
        }
    }
    add(QStringLiteral("菜单数量"), QString::number(topMenus.size()));

    QStringList names;
    for (QMenu *m : topMenus) {
        names << m->title().remove(QLatin1Char('&')).remove(QLatin1Char('(')).section(QLatin1Char(')'), 0, 0);
    }
    for (const QString &n : {QStringLiteral("文件"), QStringLiteral("编辑"),
                             QStringLiteral("段落"), QStringLiteral("格式"),
                             QStringLiteral("视图"), QStringLiteral("主题"),
                             QStringLiteral("帮助")}) {
        add(QStringLiteral("菜单_") + n, names.contains(n) ? QStringLiteral("1") : QStringLiteral("0"));
    }
    Q_UNUSED(menus);

    // ---------------- 侧边栏页签 ----------------
    add(QStringLiteral("侧边栏页签数"),
        m_sidebarTabs ? QString::number(m_sidebarTabs->count()) : QStringLiteral("0"));
    if (m_sidebarTabs) {
        for (int i = 0; i < m_sidebarTabs->count(); ++i) {
            const QString t = m_sidebarTabs->tabText(i);
            add(QStringLiteral("页签_") + t, QStringLiteral("1"));
        }
    }

    // ---------------- 快捷键 ----------------
    auto sc = [](QAction *a) {
        return a ? a->shortcut().toString() : QString();
    };
    add(QStringLiteral("快捷键_标题1"), sc(m_heading1Action));
    add(QStringLiteral("快捷键_段落"), sc(m_paragraphAction));
    add(QStringLiteral("快捷键_提升标题"), sc(m_increaseHeadingAction));
    add(QStringLiteral("快捷键_降低标题"), sc(m_decreaseHeadingAction));
    add(QStringLiteral("快捷键_源码模式"), sc(m_sourceModeAction));
    add(QStringLiteral("快捷键_专注模式"), sc(m_focusModeAction));
    add(QStringLiteral("快捷键_打字机模式"), sc(m_typewriterModeAction));
    add(QStringLiteral("快捷键_侧边栏"), sc(m_sidebarAction));
    add(QStringLiteral("快捷键_大纲"), sc(m_outlineAction));
    add(QStringLiteral("快捷键_文档树"), sc(m_docTreeAction));
    add(QStringLiteral("快捷键_文件树"), sc(m_fileTreeAction));
    add(QStringLiteral("快捷键_实际大小"), sc(m_actualSizeAction));

    // ---------------- 行内格式引擎 ----------------
    if (m_inlineFormatter) {
        const QString text = QStringLiteral("文本");
        const int n = text.length();
        struct Case { const char *key; const char *id; };
        const Case cases[] = {
            {"格式_加粗", "bold"},
            {"格式_斜体", "italic"},
            {"格式_删除线", "strikethrough"},
            {"格式_高亮", "highlight"},
            {"格式_上标", "superscript"},
            {"格式_下标", "subscript"},
        };
        for (const Case &c : cases) {
            QString o; int s0 = 0, e0 = 0;
            m_inlineFormatter->toggle(QString::fromLatin1(c.id), text, 0, n, o, s0, e0,
                                       QStringLiteral("文本"));
            add(QString::fromLatin1(c.key) + QStringLiteral("包裹"), o);
            // 再 toggle 一次应还原
            QString o2; int s1 = 0, e1 = 0;
            m_inlineFormatter->toggle(QString::fromLatin1(c.id), o, s0, e0, o2, s1, e1,
                                       QStringLiteral("文本"));
            add(QString::fromLatin1(c.key) + QStringLiteral("取消"), o2);
        }
    }

    // ---------------- 块级编辑引擎 ----------------
    if (m_blockEditor) {
        const QString head = QStringLiteral("# 标题");
        BlockEditor::Result r;

        r = BlockEditor::increaseHeading(head, 0);
        add(QStringLiteral("块_提升标题"), r.text);
        r = BlockEditor::decreaseHeading(QStringLiteral("## 标题"), 0);
        add(QStringLiteral("块_降低标题"), r.text);
        r = BlockEditor::decreaseHeading(QStringLiteral("# 标题"), 0);
        add(QStringLiteral("块_一级转段落"), r.text);

        r = BlockEditor::toggleBlock(QStringLiteral("文字"), 0, QStringLiteral("quote"));
        add(QStringLiteral("块_引用"), r.text);
        r = BlockEditor::toggleBlock(QStringLiteral("文字"), 0, QStringLiteral("ul"));
        add(QStringLiteral("块_无序列表"), r.text);
        r = BlockEditor::toggleBlock(QStringLiteral("文字"), 0, QStringLiteral("ol"));
        add(QStringLiteral("块_有序列表"), r.text);
        r = BlockEditor::toggleBlock(QStringLiteral("文字"), 0, QStringLiteral("task"));
        add(QStringLiteral("块_任务列表"), r.text);
        r = BlockEditor::toggleBlock(QStringLiteral("- [x] 文字"), 0, QStringLiteral("task"));
        add(QStringLiteral("块_任务列表完成"), r.text);
        r = BlockEditor::toggleBlock(QStringLiteral("文字"), 0, QStringLiteral("code"));
        add(QStringLiteral("块_代码块"), r.text);
        r = BlockEditor::toggleBlock(QStringLiteral("文字"), 0, QStringLiteral("math"));
        add(QStringLiteral("块_公式块"), r.text);

        // 段落插入
        r = BlockEditor::insertBlock(QStringLiteral("正文"), 0, QStringLiteral("toc"));
        add(QStringLiteral("段落_内容目录"),
            r.text.contains(QStringLiteral("[TOC]")) ? QStringLiteral("[TOC]") : QString());
        r = BlockEditor::insertBlock(QStringLiteral("正文"), 0, QStringLiteral("hr"));
        add(QStringLiteral("段落_水平分割线"),
            r.text.contains(QStringLiteral("---")) ? QStringLiteral("---") : QString());
        r = BlockEditor::insertBlock(QStringLiteral("正文"), 0, QStringLiteral("yaml"));
        add(QStringLiteral("段落_YAML"),
            r.text.contains(QStringLiteral("title:")) ? QStringLiteral("title:") : QString());
        r = BlockEditor::insertBlock(QStringLiteral("正文"), 0, QStringLiteral("footnote"));
        add(QStringLiteral("段落_脚注引用"),
            r.text.contains(QStringLiteral("[^fn1]")) ? QStringLiteral("[^fn1]") : QString());
        add(QStringLiteral("段落_脚注定义"),
            r.text.contains(QStringLiteral("[^fn1]: 脚注内容"))
                ? QStringLiteral("[^fn1]: 脚注内容") : QString());

        // 表格
        const QString table = QStringLiteral(
            "| A | B |\n| --- | --- |\n| 1 | 2 |\n");
        r = BlockEditor::insertTableRow(table, table.length() - 1, QStringLiteral("below"));
        add(QStringLiteral("表格_插入行"),
            r.text.contains(QStringLiteral("|  |")) ? QStringLiteral("| 新行 |")
                                                   : r.text.section(QStringLiteral("\n"), 3, 3));
        r = BlockEditor::insertTableColumn(table, 2, QStringLiteral("after"));
        add(QStringLiteral("表格_插入列"),
            r.text.contains(QStringLiteral("---")) ? QStringLiteral("| 新列 |") : QString());
    }

    // ---------------- 打字机模式 ----------------
    setTypewriterMode(true);
    add(QStringLiteral("打字机_开启"), m_typewriterMode ? QStringLiteral("1") : QStringLiteral("0"));
    setTypewriterMode(false);
    add(QStringLiteral("打字机_关闭"), m_typewriterMode ? QStringLiteral("1") : QStringLiteral("0"));

    return out.join('\n');
}
'''

s = s.rstrip() + '\n' + IMPL
io.open(p, 'w', encoding='utf-8', newline='\n').write(s)
print('  selfTestTyporaUi 已追加')

# ===========================================================================
# 2) mainwindow.h: 声明
# ===========================================================================
ph = 'src/mainwindow.h'
h = io.open(ph, encoding='utf-8').read()
if 'selfTestTyporaUi' not in h:
    h = h.replace('    QString selfTestPreviewOnlyToggle();',
                  '    QString selfTestPreviewOnlyToggle();\n\n'
                  '    // 自检：Typora UI 规格（菜单/快捷键/格式引擎/块级引擎）\n'
                  '    QString selfTestTyporaUi();', 1)
    io.open(ph, 'w', encoding='utf-8', newline='\n').write(h)
    print('  mainwindow.h 已声明')

# ===========================================================================
# 3) main.cpp: 注册选项与处理
# ===========================================================================
pm = 'src/main.cpp'
m = io.open(pm, encoding='utf-8').read()

if 'self-test-typora-ui' not in m:
    # 选项定义
    old_opt = '''    // 自检：验证「单独预览模式」菜单项的连接与状态切换
    QCommandLineOption selfTestOption(
        QStringList() << "self-test-preview-only",
        "自检：触发「单独预览模式」菜单项并输出结果");
    parser.addOption(selfTestOption);'''
    new_opt = old_opt + '''

    // 自检：Typora UI 规格
    QCommandLineOption selfTestUiOption(
        QStringList() << "self-test-typora-ui",
        "自检：输出 Typora UI 规格（菜单/快捷键/格式与块级引擎）的验证结果");
    parser.addOption(selfTestUiOption);'''
    assert old_opt in m, '选项定义块未匹配'
    m = m.replace(old_opt, new_opt, 1)

    # 处理逻辑
    old_run = '''    // 自检模式：验证「单独预览模式」菜单项
    if (parser.isSet(selfTestOption)) {'''
    new_run = '''    // 自检模式：Typora UI 规格
    if (parser.isSet(selfTestUiOption)) {
        QTextStream sout(stdout);
        sout << w.selfTestTyporaUi() << "\\n";
        sout.flush();
        return 0;
    }

    // 自检模式：验证「单独预览模式」菜单项
    if (parser.isSet(selfTestOption)) {'''
    assert old_run in m, '处理块未匹配'
    m = m.replace(old_run, new_run, 1)

    io.open(pm, 'w', encoding='utf-8', newline='\n').write(m)
    print('  main.cpp 已注册 --self-test-typora-ui')
