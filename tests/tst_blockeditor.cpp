/**
 * BlockEditor 单元测试。
 *
 * BlockEditor 是「段落」菜单背后的纯函数引擎：
 * 所有操作都是「整段文本 -> 新文本」的变换，因此天然可单测。
 */
#include <QtTest>

#include "blockeditor.h"

class TestBlockEditor : public QObject
{
    Q_OBJECT

private slots:
    void lineType_data();
    void lineType();
    void headingLevel_data();
    void headingLevel();
    void increaseHeading();
    void increaseHeading_fromParagraph();
    void increaseHeading_atMaxLevel();
    void decreaseHeading();
    void decreaseHeading_toParagraph();
    void decreaseHeading_notHeading();
    void toggleBlock_quote();
    void toggleBlock_ul();
    void toggleBlock_paragraph();
    void toggleBlock_toggleOff();
    void footnotes();
    void newFootnoteId();
};

// ===========================================================================
// 行类型判定
// ===========================================================================

void TestBlockEditor::lineType_data()
{
    QTest::addColumn<QString>("line");
    QTest::addColumn<int>("expected");

    QTest::newRow("空行")      << QString()                     << int(BlockEditor::Unknown);
    QTest::newRow("标题")      << QStringLiteral("# 标题")       << int(BlockEditor::Heading);
    QTest::newRow("六级标题")  << QStringLiteral("###### 六")     << int(BlockEditor::Heading);
    QTest::newRow("代码围栏")  << QStringLiteral("```")          << int(BlockEditor::CodeBlock);
    QTest::newRow("波浪围栏")  << QStringLiteral("~~~")          << int(BlockEditor::CodeBlock);
    QTest::newRow("数学块")    << QStringLiteral("$$")           << int(BlockEditor::MathBlock);
    QTest::newRow("任务项")    << QStringLiteral("- [ ] 任务")   << int(BlockEditor::TaskItem);
    QTest::newRow("已完成任务") << QStringLiteral("- [x] 任务")  << int(BlockEditor::TaskItem);
    QTest::newRow("列表项")    << QStringLiteral("- 项目")       << int(BlockEditor::ListItem);
    QTest::newRow("有序列表")  << QStringLiteral("1. 项目")      << int(BlockEditor::ListItem);
    QTest::newRow("引用")      << QStringLiteral("> 引用")       << int(BlockEditor::Quote);
    QTest::newRow("普通段落")  << QStringLiteral("正文")         << int(BlockEditor::Paragraph);
}

void TestBlockEditor::lineType()
{
    QFETCH(QString, line);
    QFETCH(int, expected);
    QCOMPARE(int(BlockEditor::lineType(line)), expected);
}

void TestBlockEditor::headingLevel_data()
{
    QTest::addColumn<QString>("line");
    QTest::addColumn<int>("expected");

    QTest::newRow("一级")   << QStringLiteral("# 一级") << 1;
    QTest::newRow("三级")   << QStringLiteral("### 三级") << 3;
    QTest::newRow("非标题") << QStringLiteral("正文") << 0;
    QTest::newRow("井号无空格") << QStringLiteral("#标题") << 0;
}

void TestBlockEditor::headingLevel()
{
    QFETCH(QString, line);
    QFETCH(int, expected);
    QCOMPARE(BlockEditor::headingLevel(line), expected);
}

// ===========================================================================
// 标题级别调整
// ===========================================================================

void TestBlockEditor::increaseHeading()
{
    const BlockEditor::Result r =
        BlockEditor::increaseHeading(QStringLiteral("# 一级"), 0);
    QVERIFY(r.changed);
    QCOMPARE(r.text, QStringLiteral("## 一级"));
}

void TestBlockEditor::increaseHeading_fromParagraph()
{
    // 非标题行会先转成一级标题
    const BlockEditor::Result r =
        BlockEditor::increaseHeading(QStringLiteral("正文"), 0);
    QVERIFY(r.changed);
    QCOMPARE(r.text, QStringLiteral("# 正文"));
}

void TestBlockEditor::increaseHeading_atMaxLevel()
{
    // 已经是六级标题就不该再提升（changed 保持 false）
    const BlockEditor::Result r =
        BlockEditor::increaseHeading(QStringLiteral("###### 六级"), 0);
    QVERIFY(!r.changed);
}

void TestBlockEditor::decreaseHeading()
{
    const BlockEditor::Result r =
        BlockEditor::decreaseHeading(QStringLiteral("### 三级"), 0);
    QVERIFY(r.changed);
    QCOMPARE(r.text, QStringLiteral("## 三级"));
}

void TestBlockEditor::decreaseHeading_toParagraph()
{
    // 一级标题降级后变回普通段落
    const BlockEditor::Result r =
        BlockEditor::decreaseHeading(QStringLiteral("# 一级"), 0);
    QVERIFY(r.changed);
    QCOMPARE(r.text, QStringLiteral("一级"));
}

void TestBlockEditor::decreaseHeading_notHeading()
{
    const BlockEditor::Result r =
        BlockEditor::decreaseHeading(QStringLiteral("正文"), 0);
    QVERIFY(!r.changed);
}
// ===========================================================================
// 块类型切换
// ===========================================================================

void TestBlockEditor::toggleBlock_quote()
{
    const BlockEditor::Result r =
        BlockEditor::toggleBlock(QStringLiteral("正文"), 0, QStringLiteral("quote"));
    QVERIFY(r.changed);
    QCOMPARE(r.text, QStringLiteral("> 正文"));
}

void TestBlockEditor::toggleBlock_ul()
{
    const BlockEditor::Result r =
        BlockEditor::toggleBlock(QStringLiteral("正文"), 0, QStringLiteral("ul"));
    QVERIFY(r.changed);
    QCOMPARE(r.text, QStringLiteral("- 正文"));
}

void TestBlockEditor::toggleBlock_paragraph()
{
    const BlockEditor::Result r =
        BlockEditor::toggleBlock(QStringLiteral("- 正文"), 0, QStringLiteral("paragraph"));
    QVERIFY(r.changed);
    QCOMPARE(r.text, QStringLiteral("正文"));
}

void TestBlockEditor::toggleBlock_toggleOff()
{
    // 已经是引用，再切一次引用应当取消引用
    const BlockEditor::Result r =
        BlockEditor::toggleBlock(QStringLiteral("> 正文"), 0, QStringLiteral("quote"));
    QVERIFY(r.changed);
    QCOMPARE(r.text, QStringLiteral("正文"));
}

// ===========================================================================
// 脚注
// ===========================================================================

void TestBlockEditor::footnotes()
{
    const QStringList ids = BlockEditor::listFootnotes(
        QStringLiteral("正文\n[^note]: 脚注内容\n[^other]: 另一个\n"));
    QCOMPARE(ids.size(), 2);
    QCOMPARE(ids.at(0), QStringLiteral("note"));
    QCOMPARE(ids.at(1), QStringLiteral("other"));
}

void TestBlockEditor::newFootnoteId()
{
    // 没有脚注时从 fn1 开始
    QCOMPARE(BlockEditor::newFootnoteId(QStringLiteral("正文")), QStringLiteral("fn1"));
    // 已存在 fn1 时避开它
    QCOMPARE(BlockEditor::newFootnoteId(QStringLiteral("[^fn1]: 定义")),
             QStringLiteral("fn2"));
}

QTEST_APPLESS_MAIN(TestBlockEditor)
#include "tst_blockeditor.moc"