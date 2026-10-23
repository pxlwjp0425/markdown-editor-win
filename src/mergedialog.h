#ifndef MERGEDIALOG_H
#define MERGEDIALOG_H

#include <QDialog>
#include <QStringList>

class QListWidget;
class QComboBox;
class QCheckBox;

/**
 * Markdown 多文件合并对话框。
 *
 * 用法（见 MainWindow::mergeMarkdown）：
 *   - 添加文件：QFileDialog 多选
 *   - 添加文件夹：递归收集目录下所有 .md
 *   - 合并顺序：列表内拖拽 / 上下移（手动），或按「文件名 / 路径 / 修改时间」自动排序
 *   - 「合并」= 结果开新标签；「合并并保存」= 结果开新标签并立即弹出保存对话框
 */
class MergeDialog : public QDialog
{
    Q_OBJECT

public:
    explicit MergeDialog(QWidget *parent = nullptr);

    // 仅当用户点击「合并」或「合并并保存」后才有意义
    QString mergedText() const { return m_merged; }
    bool saveRequested() const { return m_saveRequested; }

    // 供测试 / 批量添加：把若干路径追加进列表
    void addPaths(const QStringList &paths);

    // 触发合并计算（与「合并」按钮同逻辑，但不依赖事件循环）
    void doMerge(bool save);

private slots:
    void addFiles();
    void addFolder();
    void removeSelected();
    void moveUp();
    void moveDown();
    void applyAutoSort(int index);

private:
    void addItem(const QString &path);
    QStringList orderedFiles() const;
    void repopulate(const QStringList &files);

    QListWidget *m_list = nullptr;
    QComboBox *m_sortCombo = nullptr;
    QCheckBox *m_headingCheck = nullptr;
    QString m_merged;
    bool m_saveRequested = false;
};

#endif // MERGEDIALOG_H
