/**
 * SmartPunctuation 单元测试。
 *
 * 智能引号 / 智能破折号 / 省略号的转换规则，
 * 以及「英文缩写里的撇号不转换」这类例外。
 */
#include <QtTest>

#include "smartpunctuation.h"

class TestSmartPunctuation : public QObject
{
    Q_OBJECT

private slots:
    void disabledByDefault();
    void offMode();
    void quote_opening();
    void quote_closing();
    void apostrophe_inWord();
    void apostrophe_standalone();
    void dash_enDash();
    void dash_emDash();
    void ellipsis();
    void convertText_data();
    void convertText();
};

// ===========================================================================

void TestSmartPunctuation::disabledByDefault()
{
    SmartPunctuation p;
    QVERIFY(!p.isEnabled());

    QString replace;
    int caretBack = 0;
    QVERIFY(!p.process('"', QChar(), QChar('a'), replace, caretBack));
    QVERIFY(replace.isEmpty());
}

void TestSmartPunctuation::offMode()
{
    SmartPunctuation p;
    p.setEnabled(true);
    p.setMode(SmartPunctuation::Off);

    QString replace;
    int caretBack = 0;
    QVERIFY(!p.process('"', QChar(), QChar('a'), replace, caretBack));
}

void TestSmartPunctuation::quote_opening()
{
    SmartPunctuation p;
    p.setEnabled(true);

    QString replace;
    int caretBack = 0;
    QVERIFY(p.process('"', QChar(), QChar('a'), replace, caretBack));
    QCOMPARE(replace, QStringLiteral("“"));
}

void TestSmartPunctuation::quote_closing()
{
    SmartPunctuation p;
    p.setEnabled(true);

    QString replace;
    int caretBack = 0;
    // 前一个字符是字母 -> 闭引号
    QVERIFY(p.process('"', QChar('a'), QChar(' '), replace, caretBack));
    QCOMPARE(replace, QStringLiteral("”"));
}

void TestSmartPunctuation::apostrophe_inWord()
{
    SmartPunctuation p;
    p.setEnabled(true);

    QString replace;
    int caretBack = 0;
    // don't：前后都是字母，不该转换
    QVERIFY(!p.process('\'', QChar('n'), QChar('t'), replace, caretBack));
    QVERIFY(replace.isEmpty());
}

void TestSmartPunctuation::apostrophe_standalone()
{
    SmartPunctuation p;
    p.setEnabled(true);

    QString replace;
    int caretBack = 0;
    QVERIFY(p.process('\'', QChar(), QChar('a'), replace, caretBack));
    QCOMPARE(replace, QStringLiteral("‘"));
}

void TestSmartPunctuation::dash_enDash()
{
    SmartPunctuation p;
    p.setEnabled(true);

    QString replace;
    int caretBack = 0;
    // 第二个 -，后面不是 -
    QVERIFY(p.process('-', QChar('-'), QChar('b'), replace, caretBack));
    QCOMPARE(replace, QStringLiteral("–"));
    QCOMPARE(caretBack, 1);
}

void TestSmartPunctuation::dash_emDash()
{
    SmartPunctuation p;
    p.setEnabled(true);

    QString replace;
    int caretBack = 0;
    // 第二个 -，后面还是 -
    QVERIFY(p.process('-', QChar('-'), QChar('-'), replace, caretBack));
    QCOMPARE(replace, QStringLiteral("—"));
    QCOMPARE(caretBack, 1);
}

void TestSmartPunctuation::ellipsis()
{
    SmartPunctuation p;
    p.setEnabled(true);

    QString replace;
    int caretBack = 0;
    QVERIFY(p.process('.', QChar('.'), QChar('.'), replace, caretBack));
    QCOMPARE(replace, QStringLiteral("…"));
    QCOMPARE(caretBack, 1);
}

void TestSmartPunctuation::convertText_data()
{
    QTest::addColumn<QString>("input");
    QTest::addColumn<QString>("expected");

    QTest::newRow("双引号")   << QStringLiteral("He said \"hi\"")
                              << QStringLiteral("He said “hi”");
    QTest::newRow("英文撇号") << QStringLiteral("don't")
                              << QStringLiteral("don't");
    QTest::newRow("双横线")   << QStringLiteral("a--b")
                              << QStringLiteral("a–b");
    QTest::newRow("三横线")   << QStringLiteral("a---b")
                              << QStringLiteral("a—b");
    QTest::newRow("省略号")   << QStringLiteral("wait...")
                              << QStringLiteral("wait…");
}

void TestSmartPunctuation::convertText()
{
    QFETCH(QString, input);
    QFETCH(QString, expected);

    SmartPunctuation p;
    p.setEnabled(true);
    QCOMPARE(p.convertText(input), expected);
}

QTEST_GUILESS_MAIN(TestSmartPunctuation)
#include "tst_smartpunctuation.moc"