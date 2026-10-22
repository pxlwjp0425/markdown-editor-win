/**
 * InlineFormatter 单元测试。
 *
 * 行内格式引擎：所有「包裹型」格式的切换都是纯文本变换，
 * 所以可以在不启动任何界面的情况下完整验证。
 */
#include <QtTest>

#include "inlineformatter.h"

class TestInlineFormatter : public QObject
{
    Q_OBJECT

private slots:
    void defaultFormats();
    void spec_isAsymmetric();
    void toggle_wrapsSelection();
    void toggle_unwrapsSelection();
    void toggle_isWrapped();
    void toggle_insertsTemplate();
    void toggle_customPlaceholder();
    void toggle_stripsMarkers();
};

// ===========================================================================

void TestInlineFormatter::defaultFormats()
{
    InlineFormatter f;
    const QStringList ids = f.formatIds();

    QVERIFY(ids.contains(QStringLiteral("bold")));
    QVERIFY(ids.contains(QStringLiteral("italic")));
    QVERIFY(ids.contains(QStringLiteral("strikethrough")));
    QVERIFY(ids.contains(QStringLiteral("code")));
    QVERIFY(ids.contains(QStringLiteral("highlight")));
    QVERIFY(ids.contains(QStringLiteral("superscript")));
    QVERIFY(ids.contains(QStringLiteral("subscript")));
    QVERIFY(ids.contains(QStringLiteral("comment")));
    QVERIFY(ids.contains(QStringLiteral("math")));

    QCOMPARE(f.spec(QStringLiteral("bold")).open, QStringLiteral("**"));
    QCOMPARE(f.spec(QStringLiteral("bold")).close, QStringLiteral("**"));
}

/**
 * 注释是非对称标记：<!-- 与 --> 不同。
 */
void TestInlineFormatter::spec_isAsymmetric()
{
    InlineFormatter f;
    const InlineFormatter::Spec s = f.spec(QStringLiteral("comment"));
    QCOMPARE(s.open, QStringLiteral("<!--"));
    QCOMPARE(s.close, QStringLiteral("-->"));
    QVERIFY(!s.same);
}

void TestInlineFormatter::toggle_wrapsSelection()
{
    InlineFormatter f;
    QString out;
    int selStart = 0, selEnd = 0;
    const bool changed = f.toggle(QStringLiteral("bold"), QStringLiteral("文字"),
                                  0, 2, out, selStart, selEnd);
    QVERIFY(changed);
    QCOMPARE(out, QStringLiteral("**文字**"));
    // 选区应落在内容上，而不是标记上
    QCOMPARE(selStart, 2);
    QCOMPARE(selEnd, 4);
}

void TestInlineFormatter::toggle_unwrapsSelection()
{
    InlineFormatter f;
    QString out;
    int selStart = 0, selEnd = 0;
    const bool changed = f.toggle(QStringLiteral("bold"), QStringLiteral("**文字**"),
                                  2, 4, out, selStart, selEnd);
    QVERIFY(changed);
    QCOMPARE(out, QStringLiteral("文字"));
    QCOMPARE(selStart, 0);
    QCOMPARE(selEnd, 2);
}

void TestInlineFormatter::toggle_isWrapped()
{
    InlineFormatter f;
    QVERIFY(f.isWrapped(QStringLiteral("bold"), QStringLiteral("**文字**"), 2, 4));
    QVERIFY(!f.isWrapped(QStringLiteral("bold"), QStringLiteral("文字"), 0, 2));
}

void TestInlineFormatter::toggle_insertsTemplate()
{
    InlineFormatter f;
    QString out;
    int selStart = 0, selEnd = 0;
    const bool changed = f.toggle(QStringLiteral("bold"), QString(),
                                  0, 0, out, selStart, selEnd);
    QVERIFY(changed);
    // 无选区时插入模板，默认占位文本是「文本」
    QCOMPARE(out, QStringLiteral("**文本**"));
    QCOMPARE(selStart, 2);
    QCOMPARE(selEnd, 4);
}

void TestInlineFormatter::toggle_customPlaceholder()
{
    InlineFormatter f;
    QString out;
    int selStart = 0, selEnd = 0;
    const bool changed = f.toggle(QStringLiteral("bold"), QString(),
                                  0, 0, out, selStart, selEnd,
                                  QStringLiteral("加粗"));
    QVERIFY(changed);
    QCOMPARE(out, QStringLiteral("**加粗**"));
}

void TestInlineFormatter::toggle_stripsMarkers()
{
    // 非对称标记也必须能被正确地加上与去掉
    InlineFormatter f;
    QString out;
    int selStart = 0, selEnd = 0;

    QVERIFY(f.toggle(QStringLiteral("comment"), QStringLiteral("备注"),
                     0, 2, out, selStart, selEnd));
    QCOMPARE(out, QStringLiteral("<!--备注-->"));

    QVERIFY(f.toggle(QStringLiteral("comment"), out,
                     4, 6, out, selStart, selEnd));
    QCOMPARE(out, QStringLiteral("备注"));
}

QTEST_APPLESS_MAIN(TestInlineFormatter)
#include "tst_inlineformatter.moc"