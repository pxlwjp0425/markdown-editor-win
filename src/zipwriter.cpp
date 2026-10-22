#include "zipwriter.h"

#include <QDataStream>
#include <QFile>
#include <QDateTime>

namespace {

// CRC32 查找表（多项式 0xEDB88320，与 ZIP/PNG 一致）
struct Crc32Table {
    quint32 t[256];
    Crc32Table() {
        for (quint32 i = 0; i < 256; ++i) {
            quint32 c = i;
            for (int k = 0; k < 8; ++k) {
                c = (c & 1) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
            }
            t[i] = c;
        }
    }
};

const Crc32Table &crcTable()
{
    static const Crc32Table table;
    return table;
}

// 小端写入辅助
void putU16(QByteArray &out, quint16 v)
{
    out.append(static_cast<char>(v & 0xFF));
    out.append(static_cast<char>((v >> 8) & 0xFF));
}

void putU32(QByteArray &out, quint32 v)
{
    out.append(static_cast<char>(v & 0xFF));
    out.append(static_cast<char>((v >> 8) & 0xFF));
    out.append(static_cast<char>((v >> 16) & 0xFF));
    out.append(static_cast<char>((v >> 24) & 0xFF));
}

// DOS 时间/日期（ZIP 头用）
void dosDateTime(quint16 &dosTime, quint16 &dosDate)
{
    const QDateTime now = QDateTime::currentDateTime();
    const QTime t = now.time();
    const QDate d = now.date();
    dosTime = static_cast<quint16>((t.hour() << 11) | (t.minute() << 5) | (t.second() / 2));
    dosDate = static_cast<quint16>(((d.year() - 1980) << 9) | (d.month() << 5) | d.day());
}

}  // namespace

ZipWriter::ZipWriter() = default;

quint32 ZipWriter::crc32(const QByteArray &data)
{
    const Crc32Table &tb = crcTable();
    quint32 crc = 0xFFFFFFFFu;
    for (int i = 0; i < data.size(); ++i) {
        crc = tb.t[(crc ^ static_cast<quint8>(data.at(i))) & 0xFF] ^ (crc >> 8);
    }
    return crc ^ 0xFFFFFFFFu;
}

void ZipWriter::addFile(const QString &name, const QByteArray &data)
{
    Entry e;
    e.name = name;
    e.data = data;
    e.crc = crc32(data);
    m_entries.append(e);
}

QByteArray ZipWriter::toByteArray() const
{
    QByteArray out;
    out.reserve(64 * 1024);

    quint16 dosTime = 0, dosDate = 0;
    dosDateTime(dosTime, dosDate);

    // ---- 逐个写 Local File Header + 数据 ----
    QList<quint32> offsets;
    for (const Entry &e : m_entries) {
        const QByteArray nameUtf8 = e.name.toUtf8();
        const quint32 offset = static_cast<quint32>(out.size());
        offsets.append(offset);

        putU32(out, 0x04034B50);                       // Local file header signature
        putU16(out, 20);                               // Version needed (2.0)
        putU16(out, 0x0800);                           // Flags: bit 11 = UTF-8 文件名
        putU16(out, 0);                                // Compression: 0 = store
        putU16(out, dosTime);
        putU16(out, dosDate);
        putU32(out, e.crc);
        putU32(out, static_cast<quint32>(e.data.size()));   // Compressed size
        putU32(out, static_cast<quint32>(e.data.size()));   // Uncompressed size
        putU16(out, static_cast<quint16>(nameUtf8.size()));
        putU16(out, 0);                                // Extra field length
        out.append(nameUtf8);
        out.append(e.data);
    }

    // ---- Central Directory ----
    const quint32 centralStart = static_cast<quint32>(out.size());
    for (int i = 0; i < m_entries.size(); ++i) {
        const Entry &e = m_entries.at(i);
        const QByteArray nameUtf8 = e.name.toUtf8();

        putU32(out, 0x02014B50);                       // Central dir signature
        putU16(out, 20);                               // Version made by
        putU16(out, 20);                               // Version needed
        putU16(out, 0x0800);                           // Flags
        putU16(out, 0);                                // Compression
        putU16(out, dosTime);
        putU16(out, dosDate);
        putU32(out, e.crc);
        putU32(out, static_cast<quint32>(e.data.size()));
        putU32(out, static_cast<quint32>(e.data.size()));
        putU16(out, static_cast<quint16>(nameUtf8.size()));
        putU16(out, 0);                                // Extra length
        putU16(out, 0);                                // Comment length
        putU16(out, 0);                                // Disk number
        putU16(out, 0);                                // Internal attrs
        putU32(out, 0);                                // External attrs
        putU32(out, offsets.at(i));                    // Local header offset
        out.append(nameUtf8);
    }
    const quint32 centralSize = static_cast<quint32>(out.size()) - centralStart;

    // ---- End of Central Directory ----
    putU32(out, 0x06054B50);
    putU16(out, 0);                                    // Disk number
    putU16(out, 0);                                    // Disk with central dir
    putU16(out, static_cast<quint16>(m_entries.size()));
    putU16(out, static_cast<quint16>(m_entries.size()));
    putU32(out, centralSize);
    putU32(out, centralStart);
    putU16(out, 0);                                    // Comment length

    return out;
}

bool ZipWriter::writeTo(const QString &filePath) const
{
    QFile f(filePath);
    if (!f.open(QIODevice::WriteOnly)) {
        return false;
    }
    const QByteArray data = toByteArray();
    const qint64 written = f.write(data);
    f.close();
    return written == data.size();
}
