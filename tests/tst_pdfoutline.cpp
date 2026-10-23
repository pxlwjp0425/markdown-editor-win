#include <QTest>
#include <QByteArray>
#include <QVector>
#include <QMap>
#include "../src/pdfoutline.h"

// 构造一个最小可解析的 PDF：Catalog(3) -> Pages(2) -> Page(4), Page(5)
static QByteArray buildMinimalPdf()
{
    struct Obj { int num; QByteArray body; };
    QVector<Obj> objs;
    objs.append({1, "<< /Type /Info >>"});
    objs.append({2, "<< /Type /Pages /Kids [4 0 R 5 0 R] /Count 2 >>"});
    objs.append({3, "<< /Type /Catalog /Pages 2 0 R >>"});
    objs.append({4, "<< /Type /Page /Parent 2 0 R >>"});
    objs.append({5, "<< /Type /Page /Parent 2 0 R >>"});

    QByteArray pdf;
    QMap<int, qint64> offs;
    for (const auto &o : objs) {
        offs[o.num] = pdf.size();
        pdf += QByteArray::number(o.num) + " 0 obj\n" + o.body + "\nendobj\n";
    }
    const qint64 xrefOff = pdf.size();
    const int maxNum = 5;
    pdf += "xref\r\n0 " + QByteArray::number(maxNum + 1) + "\r\n";
    for (int n = 0; n <= maxNum; ++n) {
        if (offs.contains(n)) {
            pdf += QString("%1 00000 n\r\n").arg(offs[n], 10, 10, QChar('0')).toLatin1();
        } else {
            pdf += "0000000000 65535 f\r\n";
        }
    }
    pdf += "trailer\r\n<< /Size " + QByteArray::number(maxNum + 1)
           + " /Root 3 0 R >>\r\n";
    pdf += "startxref\r\n" + QByteArray::number(xrefOff) + "\r\n%%EOF\r\n";
    return pdf;
}

class tst_PdfOutline : public QObject
{
    Q_OBJECT
private slots:
    void parsePageObjectNumbers();
    void injectOutlineAddsBookmarks();
    void injectEmptyReturnsOriginal();
};

void tst_PdfOutline::parsePageObjectNumbers()
{
    const QByteArray pdf = buildMinimalPdf();
    const QVector<int> pages = pdfPageObjectNumbers(pdf);
    QCOMPARE(pages.size(), 2);
    QCOMPARE(pages.at(0), 4);
    QCOMPARE(pages.at(1), 5);
}

void tst_PdfOutline::injectOutlineAddsBookmarks()
{
    const QByteArray pdf = buildMinimalPdf();
    QVector<PdfOutlineEntry> entries;
    PdfOutlineEntry e1; e1.title = QStringLiteral("章一"); e1.pageIndex = 0; e1.y = 100.0;
    PdfOutlineEntry e2; e2.title = QStringLiteral("Chapter Two"); e2.pageIndex = 1; e2.y = 200.0;
    entries << e1 << e2;

    const QByteArray out = injectPdfOutline(pdf, entries, 2);
    QVERIFY(out.contains("/Outlines"));
    QVERIFY(out.contains("/Title"));
    QVERIFY(out.contains("/Count 2"));
    QVERIFY(out.contains("/First"));
    QVERIFY(out.contains("/Last"));
    QVERIFY(out.contains("/XYZ"));
    // 原始 catalog / 页面内容仍保留
    QVERIFY(out.contains("/Type /Catalog"));
    QVERIFY(out.contains("4 0 obj"));
    // 中文标题以 UTF-16BE + BOM 的十六进制文本形式出现：FE FF 7A E0 4E 00
    QVERIFY(out.contains("feff7ae04e00"));

    // 注入后仍可正确解析页数（新 trailer /Root 指向新 Catalog，且仍指向 Pages）
    const QVector<int> pages = pdfPageObjectNumbers(out);
    QCOMPARE(pages.size(), 2);
}

void tst_PdfOutline::injectEmptyReturnsOriginal()
{
    const QByteArray pdf = buildMinimalPdf();
    const QByteArray out = injectPdfOutline(pdf, {}, 2);
    QCOMPARE(out, pdf);
}

QTEST_APPLESS_MAIN(tst_PdfOutline)
#include "tst_pdfoutline.moc"
