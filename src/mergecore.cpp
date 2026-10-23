#include "mergecore.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QTextStream>
#include <algorithm>

namespace MergeCore {

QStringList collectMarkdownFiles(const QString &folder, bool recursive)
{
    QStringList result;
    if (folder.isEmpty() || !QDir(folder).exists()) {
        return result;
    }

    QDirIterator::IteratorFlags flags = QDirIterator::NoIteratorFlags;
    if (recursive) {
        flags |= QDirIterator::Subdirectories;
    }

    // 不用 QDir 的 name filter（在大小写敏感的文件系统上 *.md 匹配不到 .MD），
    // 改为遍历所有文件后按扩展名（转小写）判断，跨平台一致。
    QDirIterator it(folder, QDir::Files, flags);
    while (it.hasNext()) {
        const QString path = it.next();
        const QString suffix = QFileInfo(path).suffix().toLower();
        if (suffix == QStringLiteral("md") || suffix == QStringLiteral("markdown")) {
            result << path;
        }
    }

    // 按下单路径升序，保证顺序确定；手动重排可在 UI 中进行。
    result.sort();
    return result;
}

void sortFiles(QStringList &files, SortKey key)
{
    switch (key) {
    case SortKey::Name:
        std::sort(files.begin(), files.end(), [](const QString &a, const QString &b) {
            return QFileInfo(a).fileName() < QFileInfo(b).fileName();
        });
        break;
    case SortKey::Path:
        files.sort();
        break;
    case SortKey::ModifiedTime:
        std::sort(files.begin(), files.end(), [](const QString &a, const QString &b) {
            return QFileInfo(a).lastModified() < QFileInfo(b).lastModified();
        });
        break;
    case SortKey::None:
    default:
        break;
    }
}

QString readMarkdown(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return QString();
    }
    QTextStream in(&file);
    in.setEncoding(QStringConverter::Utf8);
    const QString content = in.readAll();
    file.close();
    return content;
}

QString mergeFiles(const QStringList &files, bool insertHeading)
{
    QString out;
    out.reserve(1024 * files.size());
    bool first = true;

    for (const QString &path : files) {
        QString text = readMarkdown(path);
        if (text.isNull()) {
            // 读取失败：跳过正常内容，留下占位说明，便于用户发现漏掉的文件。
            text = QStringLiteral("<!-- 合并失败：无法读取 %1 -->").arg(path);
        }

        if (!first) {
            out += QStringLiteral("\n\n");
        }
        first = false;

        if (insertHeading) {
            out += QStringLiteral("# ") + QFileInfo(path).fileName()
                 + QStringLiteral("\n\n");
        }

        // 去掉尾部换行/回车，避免在插入标题或文件分隔处产生多余空行；
        // 末尾统一补一个换行。
        QString body = text;
        while (!body.isEmpty()
               && (body.endsWith(QLatin1Char('\n')) || body.endsWith(QLatin1Char('\r')))) {
            body.chop(1);
        }
        out += body;
    }

    if (!out.isEmpty()) {
        out += QStringLiteral("\n");
    }
    return out;
}

} // namespace MergeCore
