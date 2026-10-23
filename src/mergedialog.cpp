#include "mergedialog.h"
#include "mergecore.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QListWidget>
#include <QComboBox>
#include <QCheckBox>
#include <QPushButton>
#include <QFileDialog>
#include <QMessageBox>
#include <QFileInfo>
#include <QLabel>

MergeDialog::MergeDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("合并 Markdown 文件"));
    setMinimumSize(560, 480);

    auto *mainLayout = new QVBoxLayout(this);

    // ---- 顶部操作按钮 ----
    auto *btnRow = new QHBoxLayout;
    auto *addFileBtn = new QPushButton(tr("添加文件..."));
    auto *addFolderBtn = new QPushButton(tr("添加文件夹..."));
    auto *removeBtn = new QPushButton(tr("移除"));
    auto *upBtn = new QPushButton(tr("上移"));
    auto *downBtn = new QPushButton(tr("下移"));
    btnRow->addWidget(addFileBtn);
    btnRow->addWidget(addFolderBtn);
    btnRow->addWidget(removeBtn);
    btnRow->addWidget(upBtn);
    btnRow->addWidget(downBtn);
    btnRow->addStretch(1);
    mainLayout->addLayout(btnRow);

    // ---- 文件列表（可拖拽重排）----
    m_list = new QListWidget(this);
    m_list->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_list->setDragDropMode(QAbstractItemView::InternalMove);
    m_list->setDefaultDropAction(Qt::MoveAction);
    m_list->setAlternatingRowColors(true);
    mainLayout->addWidget(m_list, 1);

    // ---- 排序 + 标题选项 ----
    auto *optRow = new QHBoxLayout;
    optRow->addWidget(new QLabel(tr("自动排序：")));
    m_sortCombo = new QComboBox(this);
    m_sortCombo->addItems(QStringList()
                          << tr("不自动排序（保持手动顺序）")
                          << tr("按文件名")
                          << tr("按路径")
                          << tr("按修改时间"));
    optRow->addWidget(m_sortCombo);
    m_headingCheck = new QCheckBox(tr("在每个文件前插入文件名(一级标题)"));
    m_headingCheck->setChecked(true);
    optRow->addWidget(m_headingCheck);
    optRow->addStretch(1);
    mainLayout->addLayout(optRow);

    mainLayout->addWidget(new QLabel(
        tr("提示：在列表中拖拽可手动调整合并顺序；选「自动排序」会按所选键重排。")));

    // ---- 底部：合并 / 合并并保存 / 取消 ----
    auto *actionRow = new QHBoxLayout;
    actionRow->addStretch(1);
    auto *mergeBtn = new QPushButton(tr("合并"));
    mergeBtn->setDefault(true);
    auto *mergeSaveBtn = new QPushButton(tr("合并并保存..."));
    auto *cancelBtn = new QPushButton(tr("取消"));
    actionRow->addWidget(mergeBtn);
    actionRow->addWidget(mergeSaveBtn);
    actionRow->addWidget(cancelBtn);
    mainLayout->addLayout(actionRow);

    // ---- 信号连接 ----
    connect(addFileBtn, &QPushButton::clicked, this, &MergeDialog::addFiles);
    connect(addFolderBtn, &QPushButton::clicked, this, &MergeDialog::addFolder);
    connect(removeBtn, &QPushButton::clicked, this, &MergeDialog::removeSelected);
    connect(upBtn, &QPushButton::clicked, this, &MergeDialog::moveUp);
    connect(downBtn, &QPushButton::clicked, this, &MergeDialog::moveDown);
    connect(m_sortCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MergeDialog::applyAutoSort);
    connect(mergeBtn, &QPushButton::clicked, this, [this]() { doMerge(false); });
    connect(mergeSaveBtn, &QPushButton::clicked, this, [this]() { doMerge(true); });
    connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
}

void MergeDialog::addItem(const QString &path)
{
    const QFileInfo fi(path);
    auto *item = new QListWidgetItem(fi.fileName());
    item->setData(Qt::UserRole, fi.absoluteFilePath());
    item->setToolTip(fi.absoluteFilePath());
    m_list->addItem(item);
}

void MergeDialog::addPaths(const QStringList &paths)
{
    for (const QString &p : paths) {
        addItem(p);
    }
}

void MergeDialog::addFiles()
{
    const QStringList files = QFileDialog::getOpenFileNames(
        this,
        tr("选择要合并的 Markdown 文件"),
        QString(),
        tr("Markdown Files (*.md *.markdown);;All Files (*)"));
    addPaths(files);
}

void MergeDialog::addFolder()
{
    const QString dir = QFileDialog::getExistingDirectory(
        this,
        tr("选择文件夹（递归合并其中所有 .md）"),
        QString(),
        QFileDialog::ShowDirsOnly);
    if (dir.isEmpty()) {
        return;
    }
    const QStringList files = MergeCore::collectMarkdownFiles(dir, true);
    if (files.isEmpty()) {
        QMessageBox::information(this, tr("合并"),
                                tr("该文件夹下没有找到 .md 文件。"));
        return;
    }
    addPaths(files);
}

void MergeDialog::removeSelected()
{
    const QList<QListWidgetItem *> selected = m_list->selectedItems();
    for (QListWidgetItem *item : selected) {
        delete m_list->takeItem(m_list->row(item));
    }
}

void MergeDialog::moveUp()
{
    const int row = m_list->currentRow();
    if (row <= 0) {
        return;
    }
    QListWidgetItem *item = m_list->takeItem(row);
    m_list->insertItem(row - 1, item);
    m_list->setCurrentRow(row - 1);
}

void MergeDialog::moveDown()
{
    const int row = m_list->currentRow();
    if (row < 0 || row >= m_list->count() - 1) {
        return;
    }
    QListWidgetItem *item = m_list->takeItem(row);
    m_list->insertItem(row + 1, item);
    m_list->setCurrentRow(row + 1);
}

void MergeDialog::applyAutoSort(int index)
{
    MergeCore::SortKey key = MergeCore::SortKey::None;
    switch (index) {
    case 1: key = MergeCore::SortKey::Name; break;
    case 2: key = MergeCore::SortKey::Path; break;
    case 3: key = MergeCore::SortKey::ModifiedTime; break;
    default: break;
    }
    QStringList files = orderedFiles();
    MergeCore::sortFiles(files, key);
    repopulate(files);
}

QStringList MergeDialog::orderedFiles() const
{
    QStringList files;
    for (int i = 0; i < m_list->count(); ++i) {
        QListWidgetItem *item = m_list->item(i);
        files << item->data(Qt::UserRole).toString();
    }
    return files;
}

void MergeDialog::repopulate(const QStringList &files)
{
    m_list->clear();
    for (const QString &f : files) {
        addItem(f);
    }
}

void MergeDialog::doMerge(bool save)
{
    const QStringList files = orderedFiles();
    if (files.isEmpty()) {
        QMessageBox::warning(this, tr("合并"),
                            tr("请先添加要合并的文件。"));
        return;
    }
    m_merged = MergeCore::mergeFiles(files, m_headingCheck->isChecked());
    m_saveRequested = save;
    accept();
}
