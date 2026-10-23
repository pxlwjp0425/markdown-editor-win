/**
 * MergeCore 单元测试。
 *
 * 覆盖：目录递归收集（含大小写不敏感扩展名）、三种自动排序、
 * 带 / 不带文件名标题的合并、以及读取失败文件的占位处理。
 */
#include <QtTest>

#include "mergecore.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTextStream>

class TestMerge : public QObject
{
    Q_OBJECT

private slots:
    void collect_recursive();
    void collect_caseInsensitive();
    void collect_emptyOrMissing();
    void sortByName();
    void sortByPath();
    void sortByModifiedTime();
    void sortNoneKeepsOrder();
    void merge_withHeading();
    void merge_withoutHeading();
    void merge_missingFile();
};

// ===========================================================================

static void writeFile(const QString &path, const QString &content)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile f(path);
    QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Text));
    QTextStream out(&f);
    out.setEncoding(QStringConverter::Utf8);
    out << content;
    f.close();
}

void TestMerge::collect_recursive()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    writeFile(dir.path() + "/a.md", "A");
    writeFile(dir.path() + "/sub/b.md", "B");
    writeFile(dir.path() + "/note.txt", "ignore");

    QStringList recursive = MergeCore::collectMarkdownFiles(dir.path(), true);
    QCOMPARE(recursive.size(), 2);
    QVERIFY(recursive.contains(QFileInfo(dir.path() + "/a.md").absoluteFilePath()));
    QVERIFY(recursive.contains(QFileInfo(dir.path() + "/sub/b.md").absoluteFilePath()));

    QStringList top = MergeCore::collectMarkdownFiles(dir.path(), false);
    QCOMPARE(top.size(), 1);
    QCOMPARE(top.first(), QFileInfo(dir.path() + "/a.md").absoluteFilePath());
}

void TestMerge::collect_caseInsensitive()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    writeFile(dir.path() + "/UPPER.MD", "X");
    writeFile(dir.path() + "/mixed.Markdown", "Y");

    QStringList files = MergeCore::collectMarkdownFiles(dir.path(), false);
    QCOMPARE(files.size(), 2);
}

void TestMerge::collect_emptyOrMissing()
{
    QCOMPARE(MergeCore::collectMarkdownFiles(QString(), false).size(), 0);
    QCOMPARE(MergeCore::collectMarkdownFiles("C:/no/such/dir/xyz", false).size(), 0);
}

void TestMerge::sortByName()
{
    QStringList files = {
        QFileInfo("/x/zebra.md").absoluteFilePath(),
        QFileInfo("/x/alpha.md").absoluteFilePath(),
        QFileInfo("/x/mango.md").absoluteFilePath()
    };
    MergeCore::sortFiles(files, MergeCore::SortKey::Name);
    QCOMPARE(QFileInfo(files[0]).fileName(), QStringLiteral("alpha.md"));
    QCOMPARE(QFileInfo(files[1]).fileName(), QStringLiteral("mango.md"));
    QCOMPARE(QFileInfo(files[2]).fileName(), QStringLiteral("zebra.md"));
}

void TestMerge::sortByPath()
{
    QStringList files = {
        QFileInfo("/x/b/c.md").absoluteFilePath(),
        QFileInfo("/x/a/c.md").absoluteFilePath(),
        QFileInfo("/x/a/b.md").absoluteFilePath()
    };
    MergeCore::sortFiles(files, MergeCore::SortKey::Path);
    QCOMPARE(files[0], QFileInfo("/x/a/b.md").absoluteFilePath());
    QCOMPARE(files[1], QFileInfo("/x/a/c.md").absoluteFilePath());
    QCOMPARE(files[2], QFileInfo("/x/b/c.md").absoluteFilePath());
}

void TestMerge::sortByModifiedTime()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString p1 = dir.path() + "/older.md";
    const QString p2 = dir.path() + "/newer.md";
    writeFile(p1, "1");
    writeFile(p2, "2");

    // 显式设置修改时间，避免快速创建导致的同秒分辨率问题
    const QDateTime base = QDateTime::fromString("2020-01-01T00:00:00", Qt::ISODate);
    {
        QFile f(p1);
        QVERIFY(f.open(QIODevice::ReadWrite));
        QVERIFY(f.setFileTime(base, QFile::FileModificationTime));
    }
    {
        QFile f(p2);
        QVERIFY(f.open(QIODevice::ReadWrite));
        QVERIFY(f.setFileTime(base.addSecs(3600), QFile::FileModificationTime));
    }

    QStringList files = QStringList() << p2 << p1; // 故意逆序
    MergeCore::sortFiles(files, MergeCore::SortKey::ModifiedTime);
    // 修改时间早的排在前
    QCOMPARE(files.first(), QFileInfo(p1).absoluteFilePath());
    QCOMPARE(files.last(), QFileInfo(p2).absoluteFilePath());
}

void TestMerge::sortNoneKeepsOrder()
{
    QStringList files = {QStringLiteral("c"), QStringLiteral("a"), QStringLiteral("b")};
    MergeCore::sortFiles(files, MergeCore::SortKey::None);
    QCOMPARE(files, QStringList({QStringLiteral("c"), QStringLiteral("a"), QStringLiteral("b")}));
}

void TestMerge::merge_withHeading()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString a = dir.path() + "/a.md";
    const QString b = dir.path() + "/b.md";
    writeFile(a, "内容A");
    writeFile(b, "内容B");

    const QString merged = MergeCore::mergeFiles(QStringList() << a << b, true);
    // 每个文件前插入一级标题，文件之间两个空行，末尾一个换行
    QCOMPARE(merged, QStringLiteral("# a.md\n\n内容A\n\n# b.md\n\n内容B\n"));
}

void TestMerge::merge_withoutHeading()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString a = dir.path() + "/a.md";
    const QString b = dir.path() + "/b.md";
    writeFile(a, "内容A");
    writeFile(b, "内容B");

    const QString merged = MergeCore::mergeFiles(QStringList() << a << b, false);
    QCOMPARE(merged, QStringLiteral("内容A\n\n内容B\n"));
    QVERIFY(!merged.contains("# a.md"));
}

void TestMerge::merge_missingFile()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString a = dir.path() + "/a.md";
    const QString missing = dir.path() + "/nope.md";
    writeFile(a, "内容A");

    const QString merged = MergeCore::mergeFiles(QStringList() << a << missing, true);
    // 缺失文件以注释占位，不崩溃、不抛出，且正常文件内容仍在
    QVERIFY(merged.contains("内容A"));
    QVERIFY(merged.contains("无法读取") || merged.contains("nope.md"));
}

QTEST_MAIN(TestMerge)
#include "tst_merge.moc"
