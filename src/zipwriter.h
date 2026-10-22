#ifndef ZIPWRITER_H
#define ZIPWRITER_H

#include <QByteArray>
#include <QList>
#include <QString>

/**
 * 最小 ZIP 写入器。
 *
 * 用途：构造 docx / EPUB 这类本质是「ZIP + XML」的容器格式。
 * 只用 STORE（不压缩）模式 —— 理由：
 *   1. 实现简单，没有 deflate 依赖，兼容性最好
 *   2. EPUB 规范本身就要求 mimetype 必须 stored
 *   3. 文档体积通常在几十 KB 到几 MB，不压缩完全可接受
 *
 * 不依赖任何第三方库（CRC32 自实现）。
 */
class ZipWriter
{
public:
    ZipWriter();

    /**
     * 添加一个文件。
     * @param name   ZIP 内的相对路径，如 "word/document.xml"
     * @param data   文件内容
     */
    void addFile(const QString &name, const QByteArray &data);

    /**
     * 生成完整的 ZIP 数据。
     */
    QByteArray toByteArray() const;

    /**
     * 直接写入磁盘文件。
     */
    bool writeTo(const QString &filePath) const;

    int fileCount() const { return m_entries.size(); }

    // CRC32（公开静态方法，便于单测）
    static quint32 crc32(const QByteArray &data);

private:
    struct Entry {
        QString name;
        QByteArray data;
        quint32 crc = 0;
        quint32 offset = 0;
    };

    QList<Entry> m_entries;
};

#endif // ZIPWRITER_H
