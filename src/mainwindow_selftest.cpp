/*
 * MainWindow —— 程序内置的自检钩子。
 *
 * 以 --self-test-* 命令行参数调用，输出 key=value 供 tools/ 下的
 * Python 脚本断言。之所以放进主程序而不是纯脚本，
 * 是因为要能直接触达界面内部状态（大纲树、预览区、动作表）。
 */

#include "mainwindow.h"
#include "mergedialog.h"
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QTextStream>
#include <QStandardPaths>
#include <QDateTime>
#include <QStringConverter>
#include <QTextDocumentWriter>
#include <QTemporaryDir>
#include <QPlainTextEdit>
#include <QTabWidget>

QString MainWindow::renderMarkdownToHtml(const QString &markdown)
{
    return generateHtml(markdown);
}

QString MainWindow::selfTestPreviewOnlyToggle()
{
    if (!m_previewOnlyAction) {
        return QString("FAIL: action 未创建");
    }
    const bool before = m_previewOnlyMode;
    const bool beforeChecked = m_previewOnlyAction->isChecked();

    // 触发一次
    m_previewOnlyAction->trigger();

    const bool after = m_previewOnlyMode;
    const bool afterChecked = m_previewOnlyAction->isChecked();

    QStringList result;
    result << QString("action存在=1");
    result << QString("切换前 mode=%1 checked=%2").arg(before).arg(beforeChecked);
    result << QString("切换后 mode=%1 checked=%2").arg(after).arg(afterChecked);
    result << QString("状态翻转=%1").arg(before != after);
    result << QString("勾选同步=%1").arg(beforeChecked != afterChecked);
    result << QString("编辑器隐藏=%1").arg(!m_editorTabs->isVisible());
    result << QString("侧边栏隐藏=%1").arg(!m_fileTreeDock->isVisible());
    result << QString("预览区可见=%1").arg(m_preview->isVisible());

    // 再触发一次，验证能还原
    m_previewOnlyAction->trigger();
    result << QString("还原后 mode=%1 编辑器可见=%2 侧边栏可见=%3")
                  .arg(m_previewOnlyMode)
                  .arg(m_editorTabs->isVisible())
                  .arg(m_fileTreeDock->isVisible());

    // ---- 从源码模式切入单独预览模式 ----
    // 回归点：之前这里是手动 m_sourceMode = false，只改了状态和工具栏的勾选，
    // 漏了视图菜单的勾选，也漏了把语法高亮重新打开 ——
    // 从单独预览模式退出来之后，编辑器的高亮一直是关着的。
    setSourceMode(true);
    m_previewOnlyAction->trigger();          // 进入单独预览（应自动退出源码模式）

    const bool srcModeOff = !m_sourceMode;
    const bool srcCheckedSynced = (m_sourceModeAction->isChecked() == m_sourceMode);
    bool highlightOn = true;
    for (int i = 0; i < m_editorTabs->count(); ++i) {
        QPlainTextEdit *editor = qobject_cast<QPlainTextEdit*>(m_editorTabs->widget(i));
        if (!editor) continue;
        MarkdownHighlighter *hl = editor->findChild<MarkdownHighlighter*>();
        if (hl && !hl->isHighlightingEnabled()) {
            highlightOn = false;
        }
    }
    const bool previewVisible = m_preview->isVisible();

    m_previewOnlyAction->trigger();          // 退出单独预览

    result << QString("源码模式退出=%1").arg(srcModeOff);
    result << QString("源码勾选同步=%1").arg(srcCheckedSynced);
    result << QString("高亮已恢复=%1").arg(highlightOn);
    result << QString("源码模式预览可见=%1").arg(previewVisible);

    return result.join('\n');
}

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
    add(QStringLiteral("menu_count"), QString::number(topMenus.size()));

    QStringList names;
    for (QMenu *m : topMenus) {
        // 标题形如 "文件(&F)" -> 取括号前的中文名
        QString t = m->title();
        const int amp = t.indexOf(QLatin1Char('&'));
        if (amp >= 0) {
            t.remove(amp, 1);
        }
        const int paren = t.indexOf(QLatin1Char('('));
        if (paren >= 0) {
            t = t.left(paren);
        }
        names << t.trimmed();
    }
    // 菜单名 -> ASCII key（自检输出用纯 ASCII，避免控制台编码问题）
    const QList<QPair<QString, QString>> menuKeys = {
        {QStringLiteral("文件"), QStringLiteral("file")},
        {QStringLiteral("编辑"), QStringLiteral("edit")},
        {QStringLiteral("段落"), QStringLiteral("para")},
        {QStringLiteral("格式"), QStringLiteral("format")},
        {QStringLiteral("视图"), QStringLiteral("view")},
        {QStringLiteral("主题"), QStringLiteral("theme")},
        {QStringLiteral("帮助"), QStringLiteral("help")},
    };
    for (const auto &mk : menuKeys) {
        add(QStringLiteral("menu_") + mk.second,
            names.contains(mk.first) ? QStringLiteral("1") : QStringLiteral("0"));
    }
    Q_UNUSED(menus);

    // ---------------- 侧边栏页签（独立大纲 dock 已移出，这里只剩文件树 + 文档树）----------------
    add(QStringLiteral("sidebar_tabs"),
        m_sidebarTabs ? QString::number(m_sidebarTabs->count()) : QStringLiteral("0"));
    if (m_sidebarTabs) {
        // 页签索引 -> ASCII key：0=文件树 1=文档树
        const QStringList tabKeys = {QStringLiteral("filetree"),
                                    QStringLiteral("doctree")};
        for (int i = 0; i < m_sidebarTabs->count() && i < tabKeys.size(); ++i) {
            add(QStringLiteral("tab_") + tabKeys.at(i), QStringLiteral("1"));
        }
    }
    // 独立大纲 dock（缺省开）
    // 注意：headless 自检时主窗口未 show，dock 的 isVisible() 不可靠，
    // 改用大纲开关的勾选态（缺省勾选）来反映「缺省开」。
    add(QStringLiteral("outline_dock"),
        (m_toggleOutlineAction && m_toggleOutlineAction->isChecked()) ? QStringLiteral("1") : QStringLiteral("0"));

    // ---------------- 快捷键 ----------------
    auto sc = [](QAction *a) {
        return a ? a->shortcut().toString() : QString();
    };
    add(QStringLiteral("sc_h1"), sc(m_heading1Action));
    add(QStringLiteral("sc_paragraph"), sc(m_paragraphAction));
    add(QStringLiteral("sc_inc_heading"), sc(m_increaseHeadingAction));
    add(QStringLiteral("sc_dec_heading"), sc(m_decreaseHeadingAction));
    add(QStringLiteral("sc_source_mode"), sc(m_sourceModeAction));
    add(QStringLiteral("sc_focus_mode"), sc(m_focusModeAction));
    add(QStringLiteral("sc_typewriter"), sc(m_typewriterModeAction));
    add(QStringLiteral("sc_sidebar"), sc(m_sidebarAction));
    add(QStringLiteral("sc_outline"), sc(m_outlineAction));
    add(QStringLiteral("sc_doctree"), sc(m_docTreeAction));
    add(QStringLiteral("sc_filetree"), sc(m_fileTreeAction));
    add(QStringLiteral("sc_actual_size"), sc(m_actualSizeAction));
    add(QStringLiteral("sc_close_tab"), sc(m_closeTabAction));

    // ---------------- 源码模式：勾选状态同步 + 高亮真正关闭 ----------------
    // 回归点：
    //   1. 视图菜单的「源代码模式」(Ctrl+/) 与工具栏的「源码模式」(Alt+S)
    //      是同一个开关，勾选状态必须同步 —— 之前只同步了工具栏那个。
    //   2. 源码模式下语法高亮必须真的关掉 —— 之前写的是
    //      setDocument(document())，传入同一个指针，Qt 首行就 return，等于没做。
    {
        // 先往编辑器里放一段带 Markdown 标记的文本。
        // 不然空文档本来就没有高亮格式，后面「格式已清空」的断言会空洞地通过。
        QPlainTextEdit *probeEditor = currentEditor();
        if (probeEditor) {
            probeEditor->setPlainText(
                QStringLiteral("# 标题\n\n**加粗** 与 `行内代码`\n\n- 列表项"));
        }

        // 统计文档里的高亮格式数量。
        // QSyntaxHighlighter 是用 QTextLayout 的 additional formats 实现的
        // （Qt 6 已移除 QTextFormat::AdditionalFormat 枚举）。
        auto countHighlightFormats = [this]() {
            int n = 0;
            for (int i = 0; i < m_editorTabs->count(); ++i) {
                QPlainTextEdit *editor = qobject_cast<QPlainTextEdit*>(m_editorTabs->widget(i));
                if (!editor) continue;
                QTextDocument *doc = editor->document();
                for (int b = 0; b < doc->blockCount(); ++b) {
                    QTextBlock block = doc->findBlockByNumber(b);
                    if (!block.layout()) continue;
                    n += block.layout()->formats().size();
                }
            }
            return n;
        };

        const bool before = m_sourceMode;
        const int formatsBefore = countHighlightFormats();
        // 前提断言：切换前确实有高亮格式，否则下面的「已清空」没有意义
        add(QStringLiteral("src_mode_formats_present_before"),
            formatsBefore > 0 ? QStringLiteral("1") : QStringLiteral("0"));

        toggleSourceMode();

        add(QStringLiteral("src_mode_menu_checked"),
            m_sourceModeAction->isChecked() ? QStringLiteral("1") : QStringLiteral("0"));
        add(QStringLiteral("src_mode_toolbar_checked"),
            m_toggleSourceModeAction->isChecked() ? QStringLiteral("1") : QStringLiteral("0"));

        bool highlightOff = true;
        for (int i = 0; i < m_editorTabs->count(); ++i) {
            QPlainTextEdit *editor = qobject_cast<QPlainTextEdit*>(m_editorTabs->widget(i));
            if (!editor) continue;
            MarkdownHighlighter *hl = editor->findChild<MarkdownHighlighter*>();
            if (hl && hl->isHighlightingEnabled()) {
                highlightOff = false;
            }
        }
        add(QStringLiteral("src_mode_highlight_off"),
            highlightOff ? QStringLiteral("1") : QStringLiteral("0"));

        // 更强的断言：直接检查文档里的高亮格式是否真的被清掉。
        // 只看「高亮器停用了」不足以证明界面上的颜色真的没了。
        const int formatsAfter = countHighlightFormats();
        add(QStringLiteral("src_mode_formats_cleared"),
            formatsAfter == 0 ? QStringLiteral("1") : QStringLiteral("0"));

        toggleSourceMode();   // 还原

        add(QStringLiteral("src_mode_restored"),
            (m_sourceMode == before) ? QStringLiteral("1") : QStringLiteral("0"));
        add(QStringLiteral("src_mode_unchecked_after"),
            m_sourceModeAction->isChecked() == m_sourceMode
                ? QStringLiteral("1") : QStringLiteral("0"));
    }

    // ---------------- 行内格式引擎 ----------------
    if (m_inlineFormatter) {
        const QString text = QStringLiteral("文本");
        const int n = text.length();
        struct Case { const char *key; const char *id; };
        const Case cases[] = {
            {"fmt_bold", "bold"},
            {"fmt_italic", "italic"},
            {"fmt_strike", "strikethrough"},
            {"fmt_highlight", "highlight"},
            {"fmt_sup", "superscript"},
            {"fmt_sub", "subscript"},
        };
        for (const Case &c : cases) {
            QString o; int s0 = 0, e0 = 0;
            m_inlineFormatter->toggle(QString::fromLatin1(c.id), text, 0, n, o, s0, e0,
                                       QStringLiteral("文本"));
            add(QString::fromLatin1(c.key) + QStringLiteral("_wrap"), o);
            // 再 toggle 一次应还原
            QString o2; int s1 = 0, e1 = 0;
            m_inlineFormatter->toggle(QString::fromLatin1(c.id), o, s0, e0, o2, s1, e1,
                                       QStringLiteral("文本"));
            add(QString::fromLatin1(c.key) + QStringLiteral("_unwrap"), o2);
        }
    }

    // ---------------- 块级编辑引擎 ----------------
    if (m_blockEditor) {
        const QString head = QStringLiteral("# 标题");
        BlockEditor::Result r;

        r = BlockEditor::increaseHeading(head, 0);
        add(QStringLiteral("blk_inc_heading"), r.text);
        r = BlockEditor::decreaseHeading(QStringLiteral("## 标题"), 0);
        add(QStringLiteral("blk_dec_heading"), r.text);
        r = BlockEditor::decreaseHeading(QStringLiteral("# 标题"), 0);
        add(QStringLiteral("blk_h1_to_para"), r.text);

        r = BlockEditor::toggleBlock(QStringLiteral("文字"), 0, QStringLiteral("quote"));
        add(QStringLiteral("blk_quote"), r.text);
        r = BlockEditor::toggleBlock(QStringLiteral("文字"), 0, QStringLiteral("ul"));
        add(QStringLiteral("blk_ul"), r.text);
        r = BlockEditor::toggleBlock(QStringLiteral("文字"), 0, QStringLiteral("ol"));
        add(QStringLiteral("blk_ol"), r.text);
        r = BlockEditor::toggleBlock(QStringLiteral("文字"), 0, QStringLiteral("task"));
        add(QStringLiteral("blk_task"), r.text);
        r = BlockEditor::toggleBlock(QStringLiteral("- [x] 文字"), 0, QStringLiteral("task"));
        add(QStringLiteral("blk_task_done"), r.text);
        r = BlockEditor::toggleBlock(QStringLiteral("文字"), 0, QStringLiteral("code"));
        add(QStringLiteral("blk_code"), r.text);
        r = BlockEditor::toggleBlock(QStringLiteral("文字"), 0, QStringLiteral("math"));
        add(QStringLiteral("blk_math"), r.text);

        // 段落插入
        r = BlockEditor::insertBlock(QStringLiteral("正文"), 0, QStringLiteral("toc"));
        add(QStringLiteral("para_toc"),
            r.text.contains(QStringLiteral("[TOC]")) ? QStringLiteral("[TOC]") : QString());
        r = BlockEditor::insertBlock(QStringLiteral("正文"), 0, QStringLiteral("hr"));
        add(QStringLiteral("para_hr"),
            r.text.contains(QStringLiteral("---")) ? QStringLiteral("---") : QString());
        r = BlockEditor::insertBlock(QStringLiteral("正文"), 0, QStringLiteral("yaml"));
        add(QStringLiteral("para_yaml"),
            r.text.contains(QStringLiteral("title:")) ? QStringLiteral("title:") : QString());
        r = BlockEditor::insertBlock(QStringLiteral("正文"), 0, QStringLiteral("footnote"));
        add(QStringLiteral("para_fn_ref"),
            r.text.contains(QStringLiteral("[^fn1]")) ? QStringLiteral("[^fn1]") : QString());
        add(QStringLiteral("para_fn_def"),
            r.text.contains(QStringLiteral("[^fn1]: 脚注内容"))
                ? QStringLiteral("[^fn1]: 脚注内容") : QString());

        // 表格
        const QString table = QStringLiteral(
            "| A | B |\n| --- | --- |\n| 1 | 2 |\n");
        r = BlockEditor::insertTableRow(table, table.length() - 1, QStringLiteral("below"));
        add(QStringLiteral("tbl_insert_row"),
            r.text.contains(QStringLiteral("|  |")) ? QStringLiteral("| 新行 |")
                                                   : r.text.section(QStringLiteral("\n"), 3, 3));
        r = BlockEditor::insertTableColumn(table, 2, QStringLiteral("after"));
        add(QStringLiteral("tbl_insert_col"),
            r.text.contains(QStringLiteral("---")) ? QStringLiteral("| 新列 |") : QString());
    }

    // ---------------- 打字机模式 ----------------
    setTypewriterMode(true);
    add(QStringLiteral("tw_on"), m_typewriterMode ? QStringLiteral("1") : QStringLiteral("0"));
    setTypewriterMode(false);
    add(QStringLiteral("tw_off"), m_typewriterMode ? QStringLiteral("1") : QStringLiteral("0"));

    return out.join('\n');
}

QStringList MainWindow::selfTestExportAll(const QString &outDir)
{
    QStringList report;

    QPlainTextEdit *editor = currentEditor();
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

QString MainWindow::selfTestMerge()
{
    QStringList out;
    auto add = [&](const QString &k, const QString &v) {
        out << (k + QStringLiteral("=") + v);
    };

    // 1) 准备两个临时 .md 文件
    QTemporaryDir dir;
    if (!dir.isValid()) {
        add(QStringLiteral("merge_dir"), QStringLiteral("FAIL"));
        return out.join('\n');
    }
    const QString pa = dir.filePath(QStringLiteral("a.md"));
    const QString pb = dir.filePath(QStringLiteral("b.md"));
    auto writeOne = [](const QString &p, const QString &c) {
        QFile f(p);
        if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
            return false;
        }
        QTextStream s(&f);
        s.setEncoding(QStringConverter::Utf8);
        s << c;
        f.close();
        return true;
    };
    const bool w1 = writeOne(pa, QStringLiteral("# A\n\n正文A"));
    const bool w2 = writeOne(pb, QStringLiteral("# B\n\n正文B"));
    add(QStringLiteral("merge_write"), (w1 && w2) ? QStringLiteral("OK") : QStringLiteral("FAIL"));

    // 2) 对话框：按 b、a 手动顺序添加，关闭标题插入
    MergeDialog dlg(this);
    dlg.addPaths(QStringList() << pb << pa);
    if (QCheckBox *cb = dlg.findChild<QCheckBox *>()) {
        cb->setChecked(false);
    }
    dlg.doMerge(false);
    const QString merged = dlg.mergedText();
    add(QStringLiteral("merge_dialog_ok"),
        (merged.contains(QStringLiteral("正文A")) && merged.contains(QStringLiteral("正文B")))
            ? QStringLiteral("1") : QStringLiteral("0"));
    // 手动顺序：b 在前 -> 正文B 出现在 正文A 之前
    add(QStringLiteral("merge_order_manual"),
        merged.indexOf(QStringLiteral("正文B")) < merged.indexOf(QStringLiteral("正文A"))
            ? QStringLiteral("1") : QStringLiteral("0"));

    // 3) 合并结果应生成一个标题为「合并结果」且含内容的标签
    openMergedContent(merged);
    bool titleOk = false;
    bool contentOk = false;
    for (int i = 0; i < m_editorTabs->count(); ++i) {
        if (m_editorTabs->tabText(i) == tr("合并结果")) {
            titleOk = true;
            QPlainTextEdit *ed = qobject_cast<QPlainTextEdit *>(m_editorTabs->widget(i));
            if (ed && ed->toPlainText().contains(QStringLiteral("正文A"))) {
                contentOk = true;
            }
        }
    }
    add(QStringLiteral("merge_tab_title"), titleOk ? QStringLiteral("1") : QStringLiteral("0"));
    add(QStringLiteral("merge_tab_content"), contentOk ? QStringLiteral("1") : QStringLiteral("0"));

    // 4) 自动排序（按文件名）：a 应排到 b 之前
    MergeDialog dlg2(this);
    dlg2.addPaths(QStringList() << pb << pa);
    if (QCheckBox *cb = dlg2.findChild<QCheckBox *>()) {
        cb->setChecked(false);
    }
    QMetaObject::invokeMethod(&dlg2, "applyAutoSort", Q_ARG(int, 1)); // 1 = 按文件名
    dlg2.doMerge(false);
    const QString merged2 = dlg2.mergedText();
    add(QStringLiteral("merge_autosort_name"),
        merged2.indexOf(QStringLiteral("正文A")) < merged2.indexOf(QStringLiteral("正文B"))
            ? QStringLiteral("1") : QStringLiteral("0"));

    return out.join('\n');
}
