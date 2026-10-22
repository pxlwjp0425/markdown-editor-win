/**
 * DocModel 单元测试。
 *
 * DocModel 是全项目唯一的 Markdown 解析入口 ——
 * 预览区与 10 种导出格式都从它产出的 IR 出发，
 * 所以它的行为决定了「所见」与「所得」是否一致。
 *
 * 这里锁住的是曾经出过问题的那些规则。
 */
#include <QtTest>

#include "docmodel.h"

class TestDocModel : public QObject
{
    Q_OBJECT

private slots:
    void inline_bold();
    void inline_italic();
    void inline_strike();
    void inline_code();
    void inline_code_is_not_reparsed();
    void inline_boldItalic();
    void inline_link();
    void inline_image();
    void inline_highlight();
    void block_heading();
    void block_heading_keepsHashInText();
    void block_heading_stripsClosingHashes();
    void block_list();
    void block_nestedList();
    void block_task();
    void block_quote();
    void block_codeFence();
    void block_tildeFence();
    void block_table();
    void block_tableAlignment();
    void block_horizontalRule();
    void block_math();
    void block_toc();
    void block_paragraphDoesNotSwallowOrderedList();
    void block_sourceLine();
    void headings_skipFencedCode();
    void headings_matchParse();
    void anchorBase_data();
    void anchorBase();
    void uniqueAnchor();
    void stripClosingHashes_data();
    void stripClosingHashes();
};

void TestDocModel::inline_bold()
{
    const QList<DocInline> in = DocModel::parseInlines(QStringLiteral("**粗体**"));
    QCOMPARE(in.size(), 1);
    QCOMPARE(in.at(0).text, QStringLiteral("粗体"));
    QVERIFY(in.at(0).bold);
    QVERIFY(!in.at(0).italic);
}

void TestDocModel::inline_italic()
{
    const QList<DocInline> in = DocModel::parseInlines(QStringLiteral("*斜体*"));
    QCOMPARE(in.size(), 1);
    QCOMPARE(in.at(0).text, QStringLiteral("斜体"));
    QVERIFY(in.at(0).italic);
    QVERIFY(!in.at(0).bold);
}

void TestDocModel::inline_strike()
{
    const QList<DocInline> in = DocModel::parseInlines(QStringLiteral("~~删除~~"));
    QCOMPARE(in.size(), 1);
    QCOMPARE(in.at(0).text, QStringLiteral("删除"));
    QVERIFY(in.at(0).strike);
}

void TestDocModel::inline_code()
{
    const QList<DocInline> in = DocModel::parseInlines(QStringLiteral("`代码`"));
    QCOMPARE(in.size(), 1);
    QCOMPARE(in.at(0).text, QStringLiteral("代码"));
    QVERIFY(in.at(0).code);
}
/**
 * 回归：行内代码里的 ** 不能被当成粗体标记。
 *
 * 早期预览区的实现先把行内代码抠成 <code>，
 * 再对整段文本跑一遍粗体正则 —— 结果 `**x**` 变成了真粗体。
 */
void TestDocModel::inline_code_is_not_reparsed()
{
    const QList<DocInline> in =
        DocModel::parseInlines(QStringLiteral("`**不该变粗**`"));
    QCOMPARE(in.size(), 1);
    QCOMPARE(in.at(0).text, QStringLiteral("**不该变粗**"));
    QVERIFY(in.at(0).code);
    QVERIFY(!in.at(0).bold);
}

void TestDocModel::inline_boldItalic()
{
    const QList<DocInline> in = DocModel::parseInlines(QStringLiteral("***粗斜***"));
    QCOMPARE(in.size(), 1);
    QCOMPARE(in.at(0).text, QStringLiteral("粗斜"));
    QVERIFY(in.at(0).bold);
    QVERIFY(in.at(0).italic);
}

void TestDocModel::inline_link()
{
    const QList<DocInline> in =
        DocModel::parseInlines(QStringLiteral("[链接](https://a.com)"));
    QCOMPARE(in.size(), 1);
    QCOMPARE(in.at(0).text, QStringLiteral("链接"));
    QCOMPARE(in.at(0).link, QStringLiteral("https://a.com"));
}

void TestDocModel::inline_image()
{
    const QList<DocInline> in =
        DocModel::parseInlines(QStringLiteral("![替代文字](a.png)"));
    QCOMPARE(in.size(), 1);
    QCOMPARE(in.at(0).imageSrc, QStringLiteral("a.png"));
    QCOMPARE(in.at(0).imageAlt, QStringLiteral("替代文字"));
}

void TestDocModel::inline_highlight()
{
    const QList<DocInline> in = DocModel::parseInlines(QStringLiteral("==高亮=="));
    QCOMPARE(in.size(), 1);
    QCOMPARE(in.at(0).text, QStringLiteral("高亮"));
    QVERIFY(in.at(0).highlight);
}

void TestDocModel::block_heading()
{
    const QList<DocBlock> b = DocModel::parse(QStringLiteral("# 一级\n\n## 二级"));
    QCOMPARE(b.size(), 2);
    QCOMPARE(b.at(0).type, DocBlock::Heading);
    QCOMPARE(b.at(0).level, 1);
    QCOMPARE(b.at(0).raw, QStringLiteral("一级"));
    QCOMPARE(b.at(1).level, 2);
}

/**
 * 回归：标题文本里的 # 不能被当成闭合标记剥掉。
 *
 * 早期用 while (endsWith('#')) chop(1)，把 "## C#" 剥成了 "C"。
 */
void TestDocModel::block_heading_keepsHashInText()
{
    const QList<DocBlock> b = DocModel::parse(QStringLiteral("## C# 编程"));
    QCOMPARE(b.size(), 1);
    QCOMPARE(b.at(0).type, DocBlock::Heading);
    QCOMPARE(b.at(0).raw, QStringLiteral("C# 编程"));
}

void TestDocModel::block_heading_stripsClosingHashes()
{
    const QList<DocBlock> b = DocModel::parse(QStringLiteral("## 闭合标题 ##"));
    QCOMPARE(b.size(), 1);
    QCOMPARE(b.at(0).raw, QStringLiteral("闭合标题"));
}
void TestDocModel::block_list()
{
    const QList<DocBlock> b = DocModel::parse(QStringLiteral("- 甲\n- 乙"));
    QCOMPARE(b.size(), 2);
    QCOMPARE(b.at(0).type, DocBlock::ListItem);
    QVERIFY(!b.at(0).ordered);
    QCOMPARE(b.at(0).level, 0);
}

void TestDocModel::block_nestedList()
{
    const QList<DocBlock> b = DocModel::parse(QStringLiteral("- 父项\n  - 子项"));
    QCOMPARE(b.size(), 2);
    QCOMPARE(b.at(0).level, 0);
    QCOMPARE(b.at(1).level, 1);
}

void TestDocModel::block_task()
{
    const QList<DocBlock> b =
        DocModel::parse(QStringLiteral("- [ ] 未完成\n- [x] 已完成"));
    QCOMPARE(b.size(), 2);
    QVERIFY(b.at(0).isTask);
    QVERIFY(!b.at(0).checked);
    QVERIFY(b.at(1).isTask);
    QVERIFY(b.at(1).checked);
}

void TestDocModel::block_quote()
{
    const QList<DocBlock> b = DocModel::parse(QStringLiteral("> 引用内容"));
    QCOMPARE(b.size(), 1);
    QCOMPARE(b.at(0).type, DocBlock::Quote);
}

void TestDocModel::block_codeFence()
{
    const QList<DocBlock> b =
        DocModel::parse(QStringLiteral("```js\nvar x = 1;\n```"));
    QCOMPARE(b.size(), 1);
    QCOMPARE(b.at(0).type, DocBlock::CodeBlock);
    QCOMPARE(b.at(0).lang, QStringLiteral("js"));
    QCOMPARE(b.at(0).codeLines.size(), 1);
}

/**
 * 回归：~~~ 围栏代码块必须被识别。
 *
 * 早期预览区的主循环只认 ```，~~~ 块里的 # 会被当成标题渲染。
 */
void TestDocModel::block_tildeFence()
{
    const QList<DocBlock> b =
        DocModel::parse(QStringLiteral("~~~text\n# 这不是标题\n~~~"));
    QCOMPARE(b.size(), 1);
    QCOMPARE(b.at(0).type, DocBlock::CodeBlock);
    QCOMPARE(b.at(0).lang, QStringLiteral("text"));
    QCOMPARE(b.at(0).codeLines.size(), 1);
    QCOMPARE(b.at(0).codeLines.at(0), QStringLiteral("# 这不是标题"));
}

void TestDocModel::block_table()
{
    const QList<DocBlock> b = DocModel::parse(
        QStringLiteral("| A | B |\n|---|---|\n| 1 | 2 |"));
    QCOMPARE(b.size(), 1);
    QCOMPARE(b.at(0).type, DocBlock::Table);
    QCOMPARE(b.at(0).tableRows.size(), 2);
    // 回归：早期用 split('|') 不剥首尾的 |，多出两个空列
    QCOMPARE(b.at(0).tableRows.at(0).size(), 2);
    QCOMPARE(b.at(0).tableRows.at(0).at(0), QStringLiteral("A"));
    QCOMPARE(b.at(0).tableRows.at(0).at(1), QStringLiteral("B"));
    QCOMPARE(b.at(0).tableRows.at(1).size(), 2);
}

/**
 * 回归：:-: 这种只有一个横线的 GFM 分隔行也必须被识别为表格。
 */
void TestDocModel::block_tableAlignment()
{
    const QList<DocBlock> b = DocModel::parse(
        QStringLiteral("| 左 | 中 | 右 |\n|:--|:-:|--:|\n| 1 | 2 | 3 |"));
    QCOMPARE(b.size(), 1);
    QCOMPARE(b.at(0).type, DocBlock::Table);
    QCOMPARE(b.at(0).tableAligns.size(), 3);
    QCOMPARE(b.at(0).tableAligns.at(0), QStringLiteral("left"));
    QCOMPARE(b.at(0).tableAligns.at(1), QStringLiteral("center"));
    QCOMPARE(b.at(0).tableAligns.at(2), QStringLiteral("right"));
}

void TestDocModel::block_horizontalRule()
{
    const QList<DocBlock> b = DocModel::parse(QStringLiteral("---"));
    QCOMPARE(b.size(), 1);
    QCOMPARE(b.at(0).type, DocBlock::HorizontalRule);
}

void TestDocModel::block_math()
{
    const QList<DocBlock> b = DocModel::parse(QStringLiteral("$$\nE = mc^2\n$$"));
    QCOMPARE(b.size(), 1);
    QCOMPARE(b.at(0).type, DocBlock::MathBlock);
}

void TestDocModel::block_toc()
{
    const QList<DocBlock> b = DocModel::parse(QStringLiteral("[TOC]\n\n# 标题"));
    QCOMPARE(b.size(), 2);
    QCOMPARE(b.at(0).type, DocBlock::Toc);
}

/**
 * 回归：段落不能把紧随其后的有序列表吞进去。
 */
void TestDocModel::block_paragraphDoesNotSwallowOrderedList()
{
    const QList<DocBlock> b = DocModel::parse(QStringLiteral("段落\n1. 第一"));
    QCOMPARE(b.size(), 2);
    QCOMPARE(b.at(0).type, DocBlock::Paragraph);
    QCOMPARE(b.at(1).type, DocBlock::ListItem);
    QVERIFY(b.at(1).ordered);
}

/**
 * 源码行号：任务列表的 #md-task:N 锚点靠它定位到源码行。
 */
void TestDocModel::block_sourceLine()
{
    const QString md = QStringLiteral("# 标题\n\n- [ ] 任务");
    const QList<DocBlock> b = DocModel::parse(md);
    QCOMPARE(b.size(), 2);
    QCOMPARE(b.at(0).sourceLine, 0);
    QCOMPARE(b.at(1).sourceLine, 2);
}
void TestDocModel::headings_skipFencedCode()
{
    QList<int> lines, levels;
    QList<QString> texts;
    DocModel::collectHeadings(
        QStringLiteral("# 真标题\n\n```\n# 假标题\n```\n"), lines, levels, texts);
    QCOMPARE(texts.size(), 1);
    QCOMPARE(texts.at(0), QStringLiteral("真标题"));
}

/**
 * 大纲面板、预览区、[TOC] 三处都用同一份标题识别规则，
 * 识别出的标题集合必须一致。
 */
void TestDocModel::headings_matchParse()
{
    const QString md = QStringLiteral(
        "# 一级\n\n正文\n\n## 二级 ##\n\n```\n# 块内\n```\n\n### 三级\n");
    QList<int> lines, levels;
    QList<QString> texts;
    DocModel::collectHeadings(md, lines, levels, texts);

    QStringList fromParse;
    const QList<DocBlock> b = DocModel::parse(md);
    for (const DocBlock &block : b) {
        if (block.type == DocBlock::Heading) {
            fromParse << block.raw;
        }
    }

    QCOMPARE(texts.size(), fromParse.size());
    for (int i = 0; i < texts.size(); ++i) {
        QCOMPARE(texts.at(i), fromParse.at(i));
    }
}

void TestDocModel::anchorBase_data()
{
    QTest::addColumn<QString>("input");
    QTest::addColumn<QString>("expected");

    QTest::newRow("简单英文") << QStringLiteral("Hello World")
                              << QStringLiteral("hello-world");
    QTest::newRow("含 C#") << QStringLiteral("C# 编程")
                           << QStringLiteral("c-编程");
    QTest::newRow("首尾符号") << QStringLiteral("**加粗** 标题")
                              << QStringLiteral("加粗-标题");
    QTest::newRow("全中文") << QStringLiteral("标题")
                            << QStringLiteral("标题");
}

void TestDocModel::anchorBase()
{
    QFETCH(QString, input);
    QFETCH(QString, expected);
    QCOMPARE(DocModel::anchorBase(input), expected);
}

void TestDocModel::uniqueAnchor()
{
    QMap<QString, int> used;
    QCOMPARE(DocModel::uniqueAnchor(QStringLiteral("标题"), used),
             QStringLiteral("标题"));
    QCOMPARE(DocModel::uniqueAnchor(QStringLiteral("标题"), used),
             QStringLiteral("标题-1"));
    QCOMPARE(DocModel::uniqueAnchor(QStringLiteral("标题"), used),
             QStringLiteral("标题-2"));
    QCOMPARE(DocModel::uniqueAnchor(QStringLiteral("其他"), used),
             QStringLiteral("其他"));
}

void TestDocModel::stripClosingHashes_data()
{
    QTest::addColumn<QString>("input");
    QTest::addColumn<QString>("expected");

    QTest::newRow("C# 不剥") << QStringLiteral("C#") << QStringLiteral("C#");
    QTest::newRow("空白后闭合") << QStringLiteral("闭合 ##") << QStringLiteral("闭合");
    QTest::newRow("整段都是井号") << QStringLiteral("##") << QString();
    QTest::newRow("普通文本") << QStringLiteral("标题") << QStringLiteral("标题");
}

void TestDocModel::stripClosingHashes()
{
    QFETCH(QString, input);
    QFETCH(QString, expected);
    QCOMPARE(DocModel::stripClosingHashes(input), expected);
}

QTEST_APPLESS_MAIN(TestDocModel)
#include "tst_docmodel.moc"