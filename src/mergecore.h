#ifndef MERGECORE_H
#define MERGECORE_H

#include <QString>
#include <QStringList>

/**
 * 多文件 Markdown 合并核心逻辑。
 *
 * 只依赖 QtCore，不碰任何 GUI 部件，因此可以单独编译进 mdcore
 * 静态库并被单元测试直接链接。
 *
 * 合并场景（见 MergeDialog / MainWindow::mergeMarkdown）：
 *   - 按文件添加（QFileDialog 多选）
 *   - 按文件夹递归添加（收集目录下所有 .md）
 *   - 合并顺序：手动拖拽 / 上下移，或按「文件名 / 路径 / 修改时间」自动排序
 *   - 每个文件前可插入其文件名作为一级标题，文件之间以两个空行分隔
 */
namespace MergeCore {

// 自动排序键
enum class SortKey {
    None,          // 保持当前（手动）顺序
    Name,          // 按文件名字（短名）排序
    Path,          // 按完整路径排序
    ModifiedTime   // 按文件修改时间排序
};

// 递归收集目录下所有 Markdown 文件（扩展名大小写不敏感：.md / .markdown）。
// recursive=false 时只收顶层目录。返回绝对路径，并按下单路径升序。
// 文件夹不存在或为空时返回空列表。
QStringList collectMarkdownFiles(const QString &folder, bool recursive);

// 按指定键对文件列表原地排序。None 表示保持原顺序（用于手动排序后取消自动）。
void sortFiles(QStringList &files, SortKey key);

// 以 UTF-8 读取单个 md 文件（兼容 UTF-8 BOM）。读取失败返回空字符串。
QString readMarkdown(const QString &path);

// 合并多个 md 文件内容。
// insertHeading=true 时，在每个文件内容前插入 "# <文件名>" 一级标题；
// 各文件之间以两个空行分隔。读取失败的文件以 HTML 注释占位，便于用户发现。
QString mergeFiles(const QStringList &files, bool insertHeading);

} // namespace MergeCore

#endif // MERGECORE_H
