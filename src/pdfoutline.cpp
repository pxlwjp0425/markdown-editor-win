#include "pdfoutline.h"

#include <QRegularExpression>
#include <QMap>
#include <QPair>

namespace {

// 在 PDF 末尾附近定位 startxref 指向的 xref 偏移
qint64 findStartXref(const QByteArray &pdf)
{
    int idx = pdf.lastIndexOf("startxref");
    if (idx < 0) {
        return -1;
    }
    int p = idx + static_cast<int>(sizeof("startxref") - 1);
    while (p < pdf.size() && (pdf.at(p) == ' ' || pdf.at(p) == '\r' || pdf.at(p) == '\n')) {
        ++p;
    }
    QByteArray num;
    while (p < pdf.size() && pdf.at(p) >= '0' && pdf.at(p) <= '9') {
        num.append(pdf.at(p));
        ++p;
    }
    bool ok = false;
    const qint64 off = num.toLongLong(&ok);
    return ok ? off : -1;
}

// 解析经典 xref 表，返回 <对象号, 字节偏移>
QMap<int, qint64> parseXref(const QByteArray &pdf, qint64 xrefOff)
{
    QMap<int, qint64> map;
    int p = static_cast<int>(xrefOff);
    if (pdf.mid(p, 4) == "xref") {
        p += 4;
    }
    // 经典 xref 可能由多个子段组成：每段以 "<start> <count>" 开头
    for (;;) {
        while (p < pdf.size()
               && (pdf.at(p) == ' ' || pdf.at(p) == '\r'
                   || pdf.at(p) == '\n' || pdf.at(p) == '\t')) {
            ++p;
        }
        QByteArray a, b;
        while (p < pdf.size() && pdf.at(p) >= '0' && pdf.at(p) <= '9') {
            a.append(pdf.at(p));
            ++p;
        }
        while (p < pdf.size() && (pdf.at(p) == ' ' || pdf.at(p) == '\t')) {
            ++p;
        }
        while (p < pdf.size() && pdf.at(p) >= '0' && pdf.at(p) <= '9') {
            b.append(pdf.at(p));
            ++p;
        }
        if (a.isEmpty() || b.isEmpty()) {
            break;
        }
        const int start = a.toInt();
        const int count = b.toInt();
        while (p < pdf.size() && (pdf.at(p) == '\r' || pdf.at(p) == '\n')) {
            ++p;
        }
        for (int i = 0; i < count; ++i) {
            if (p + 20 > pdf.size()) {
                break;
            }
            // 每个条目 20 字节：10 位偏移 + 空格 + 5 位代 + 空格 + 类型 + 2 字节 EOL
            const QByteArray entry = pdf.mid(p, 20);
            const QByteArray offStr = entry.left(10);
            const char type = entry.at(17);  // 'n' 或 'f'
            bool ok = false;
            const qint64 off = offStr.toLongLong(&ok);
            if (ok && type == 'n') {
                map.insert(start + i, off);
            }
            p += 20;
        }
        // 是否还有下一个子段（紧接着的数字）
        int q = p;
        while (q < pdf.size()
               && (pdf.at(q) == ' ' || pdf.at(q) == '\r'
                   || pdf.at(q) == '\n' || pdf.at(q) == '\t')) {
            ++q;
        }
        if (q < pdf.size() && pdf.at(q) >= '0' && pdf.at(q) <= '9') {
            continue;
        }
        break;
    }
    return map;
}

// 经典 PDF 结构：[对象][xref][trailer][startxref]，trailer 在 xref 之后。
// 从 xref 偏移向后找 trailer，读取其中的 /Root。
int findRoot(const QByteArray &pdf, qint64 xrefOff)
{
    const int t = pdf.indexOf("trailer", static_cast<int>(xrefOff));
    if (t < 0) {
        return -1;
    }
    const int end = pdf.indexOf("startxref", t);
    const QByteArray seg = (end > t) ? pdf.mid(t, end - t) : pdf.mid(t, 400);
    const int i = seg.indexOf("/Root");
    if (i < 0) {
        return -1;
    }
    const QRegularExpression re("(\\d+)\\s+\\d+\\s+R");
    const auto m = re.match(seg.mid(i + 5));
    return m.hasMatch() ? m.captured(1).toInt() : -1;
}

// 读取某对象偏移处的字典（处理嵌套 << >>）
QByteArray readObjectDict(const QByteArray &pdf, qint64 off)
{
    const int start = pdf.indexOf("<<", static_cast<int>(off));
    if (start < 0) {
        return QByteArray();
    }
    int depth = 0;
    int i = start;
    for (; i < pdf.size(); ++i) {
        if (pdf.at(i) == '<' && i + 1 < pdf.size() && pdf.at(i + 1) == '<') {
            ++depth;
            ++i;
        } else if (pdf.at(i) == '>' && i + 1 < pdf.size() && pdf.at(i + 1) == '>') {
            --depth;
            ++i;
            if (depth == 0) {
                ++i;
                break;
            }
        }
    }
    return pdf.mid(start, i - start);
}

// 递归收集页面对象编号（兼容 /Pages 多级嵌套）
void collectPages(const QByteArray &pdf,
                  const QMap<int, qint64> &offsets,
                  int objNum,
                  int depth,
                  QVector<int> &pages)
{
    if (depth > 12 || !offsets.contains(objNum)) {
        return;
    }
    const QByteArray dict = readObjectDict(pdf, offsets.value(objNum));
    const bool isPage = dict.contains("/Type /Page") || dict.contains("/Type/Page");
    QVector<int> kids;
    const int k = dict.indexOf("/Kids");
    if (k >= 0) {
        const int br = dict.indexOf('[', k);
        const int brEnd = dict.indexOf(']', br);
        if (br >= 0 && brEnd > br) {
            const QByteArray arr = dict.mid(br + 1, brEnd - br - 1);
            const QRegularExpression re("(\\d+)\\s+\\d+\\s+R");
            auto it = re.globalMatch(arr);
            while (it.hasNext()) {
                kids.append(it.next().captured(1).toInt());
            }
        }
    }
    if (isPage && kids.isEmpty()) {
        pages.append(objNum);
        return;
    }
    for (int kd : kids) {
        collectPages(pdf, offsets, kd, depth + 1, pages);
    }
}

// 把 Unicode 标题编码成 PDF 文本字符串（UTF-16BE + BOM 的十六进制表示）
QByteArray toPdfTitleHex(const QString &title)
{
    QByteArray bom;
    bom.append(char(0xFE));
    bom.append(char(0xFF));
    const ushort *u = title.utf16();
    const int len = title.length();
    for (int i = 0; i < len; ++i) {
        const ushort c = u[i];
        bom.append(char((c >> 8) & 0xFF));
        bom.append(char(c & 0xFF));
    }
    QByteArray hex;
    hex.reserve(bom.size() * 2);
    const char *hexd = "0123456789abcdef";
    for (char b : bom) {
        const quint8 v = static_cast<quint8>(b);
        hex.append(hexd[v >> 4]);
        hex.append(hexd[v & 0xF]);
    }
    return "<" + hex + ">";
}

} // namespace

QVector<int> pdfPageObjectNumbers(const QByteArray &pdf)
{
    const qint64 sx = findStartXref(pdf);
    if (sx < 0) {
        return {};
    }
    const auto offsets = parseXref(pdf, sx);
    if (offsets.isEmpty()) {
        return {};
    }
    const int root = findRoot(pdf, sx);
    if (root < 0 || !offsets.contains(root)) {
        return {};
    }
    const QByteArray catDict = readObjectDict(pdf, offsets.value(root));
    int pagesObj = -1;
    const QRegularExpression reP("/Pages\\s+(\\d+)\\s+\\d+\\s+R");
    const auto m = reP.match(catDict);
    if (m.hasMatch()) {
        pagesObj = m.captured(1).toInt();
    }
    QVector<int> pages;
    if (pagesObj > 0) {
        collectPages(pdf, offsets, pagesObj, 0, pages);
    }
    return pages;
}

QByteArray injectPdfOutline(const QByteArray &pdf,
                            const QVector<PdfOutlineEntry> &entries,
                            int pageCount)
{
    if (entries.isEmpty()) {
        return pdf;  // 无标题则不注入空大纲
    }

    const qint64 sx = findStartXref(pdf);
    if (sx < 0) {
        return pdf;
    }
    auto offsets = parseXref(pdf, sx);
    if (offsets.isEmpty()) {
        return pdf;
    }
    const int root = findRoot(pdf, sx);
    if (root < 0 || !offsets.contains(root)) {
        return pdf;
    }
    const QByteArray catDict = readObjectDict(pdf, offsets.value(root));

    // 找到 /Pages，收集页面对象编号
    int pagesObj = -1;
    {
        const QRegularExpression reP("/Pages\\s+(\\d+)\\s+\\d+\\s+R");
        const auto m = reP.match(catDict);
        if (m.hasMatch()) {
            pagesObj = m.captured(1).toInt();
        }
    }
    QVector<int> pageObjNums;
    if (pagesObj > 0) {
        collectPages(pdf, offsets, pagesObj, 0, pageObjNums);
    }
    const int N = pageObjNums.isEmpty() ? pageCount : pageObjNums.size();
    if (N <= 0) {
        return pdf;
    }

    // 分配新对象编号
    int maxObj = 0;
    for (auto it = offsets.constBegin(); it != offsets.constEnd(); ++it) {
        maxObj = qMax(maxObj, it.key());
    }
    const int newCat = maxObj + 1;
    const int outlineRoot = maxObj + 2;
    QVector<int> itemNums;
    itemNums.reserve(entries.size());
    for (int i = 0; i < entries.size(); ++i) {
        itemNums.append(maxObj + 3 + i);
    }

    // 拼接新对象，并记录每个新对象在 result 中的偏移（= sx + 在 newObjs 中的位置）
    QByteArray newObjs;
    QVector<QPair<int, qint64>> newObjOffsets;
    auto addObj = [&](int num, const QByteArray &body) {
        const qint64 off = sx + newObjs.size();
        newObjs += QByteArray::number(num) + " 0 obj\n" + body + "\nendobj\n";
        newObjOffsets.append(qMakePair(num, off));
    };

    // 1) 新 Catalog：复制原 Catalog 字典并追加 /Outlines
    QByteArray newCatDict = catDict;
    const int lastClose = newCatDict.lastIndexOf(">>");
    if (lastClose >= 0) {
        newCatDict = newCatDict.left(lastClose)
                     + " /Outlines " + QByteArray::number(outlineRoot) + " 0 R >>";
    }
    addObj(newCat, newCatDict);

    // 2) 大纲根
    QByteArray orDict = "<< /Type /Outlines";
    if (!itemNums.isEmpty()) {
        orDict += " /First " + QByteArray::number(itemNums.first()) + " 0 R";
        orDict += " /Last " + QByteArray::number(itemNums.last()) + " 0 R";
        orDict += " /Count " + QByteArray::number(itemNums.size());
    }
    orDict += " >>";
    addObj(outlineRoot, orDict);

    // 3) 各条目（双向链表）
    for (int i = 0; i < entries.size(); ++i) {
        const PdfOutlineEntry &e = entries.at(i);
        const int pg = qBound(0, e.pageIndex, N - 1);
        const int pageObj = pageObjNums.at(pg);
        const QByteArray dest = "[ " + QByteArray::number(pageObj)
                               + " 0 R /XYZ 0 "
                               + QByteArray::number(e.y, 'f', 2)
                               + " 0 ]";
        QByteArray item = "<< /Title " + toPdfTitleHex(e.title)
                          + " /Parent " + QByteArray::number(outlineRoot) + " 0 R"
                          + " /Dest " + dest;
        if (i > 0) {
            item += " /Prev " + QByteArray::number(itemNums.at(i - 1)) + " 0 R";
        }
        if (i + 1 < entries.size()) {
            item += " /Next " + QByteArray::number(itemNums.at(i + 1)) + " 0 R";
        }
        item += " >>";
        addObj(itemNums.at(i), item);
    }

    // 4) 重写 xref 表 + trailer
    // 合并新旧对象偏移
    QMap<int, qint64> allOffsets = offsets;
    for (const auto &pr : newObjOffsets) {
        allOffsets.insert(pr.first, pr.second);
    }
    int maxNum = newCat;
    for (auto it = allOffsets.constBegin(); it != allOffsets.constEnd(); ++it) {
        maxNum = qMax(maxNum, it.key());
    }
    const int total = maxNum + 1;  // 对象编号 0..maxNum

    const qint64 newXrefOffset = sx + newObjs.size();
    QByteArray xref;
    xref += "xref\r\n0 " + QByteArray::number(total) + "\r\n";
    auto fmtEntry = [](qint64 off, bool inUse) -> QByteArray {
        QByteArray s;
        s += QString("%1").arg(off, 10, 10, QChar('0')).toLatin1();
        s += ' ';
        s += inUse ? QByteArray("00000 n") : QByteArray("65535 f");
        s += "\r\n";
        return s;
    };
    for (int n = 0; n < total; ++n) {
        if (allOffsets.contains(n)) {
            xref += fmtEntry(allOffsets.value(n), true);
        } else {
            xref += fmtEntry(0, false);
        }
    }

    QByteArray trailer;
    trailer += "trailer\r\n<< /Size " + QByteArray::number(total)
               + " /Root " + QByteArray::number(newCat) + " 0 R"
               + " /Prev " + QByteArray::number(sx) + " >>\r\n";
    trailer += "startxref\r\n" + QByteArray::number(newXrefOffset) + "\r\n%%EOF\r\n";

    QByteArray result = pdf.left(static_cast<int>(sx)) + newObjs + xref + trailer;
    return result;
}
