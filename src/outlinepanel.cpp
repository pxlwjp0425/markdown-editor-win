#include "outlinepanel.h"
#include "theme.h"
#include "docmodel.h"

#include <QVBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QKeyEvent>
#include <QRegularExpression>
#include <QTreeWidgetItemIterator>
#include <QScrollBar>
#include <QFile>
#include <QTextStream>
#include <QStringConverter>

OutlinePanel::OutlinePanel(QWidget *parent)
    : QWidget(parent)
    , m_currentLine(-1)
{
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    m_tree = new QTreeWidget(this);
    m_tree->setHeaderHidden(true);
    m_tree->setIndentation(14);
    m_tree->setAnimated(true);
    m_tree->setUniformRowHeights(true);
    m_tree->setExpandsOnDoubleClick(false);
    m_tree->setFocusPolicy(Qt::StrongFocus);
    m_tree->setIndentation(14);
    layout->addWidget(m_tree);


    connect(m_tree, &QTreeWidget::itemClicked, this, &OutlinePanel::onItemClicked);
    connect(m_tree, &QTreeWidget::itemActivated, this, &OutlinePanel::onItemActivated);

    applyTheme(Theme::ThemeId::Default);
}

/**
 * 收集 Markdown 中的 ATX 标题。
 *
 * 规则只有一份实现（DocModel::collectHeadings）：
 *  - 行首 1~6 个 # 后跟空白
 *  - 忽略围栏代码块内部的 # 行
 *  - 支持行尾可选的闭合 ###（标题末尾的 # 会被丢弃）
 *
 * 早期这里自己写了一套正则，与预览区那套已经漂移：
 * 大纲扫描的是「未 trim 的行」，预览扫描的是「trim 后的行」，
 * 于是缩进的标题在大纲里不出现、在预览里却出现。
 */
void OutlinePanel::collectHeadings(const QString &markdown,
                                   QList<int> &lines,
                                   QList<int> &levels,
                                   QList<QString> &texts)
{
    DocModel::collectHeadings(markdown, lines, levels, texts);
}

QString OutlinePanel::makeUniqueAnchor(const QString &base, QMap<QString, int> &used)
{
    // 锚点规则只有一份实现（DocModel::uniqueAnchor），
    // 否则大纲点击跳转与 [TOC] 链接会对不上。
    return DocModel::uniqueAnchor(base, used);
}

void OutlinePanel::dumpOutline(const QString &markdown, QStringList &out)
{
    QList<int> lines, levels;
    QList<QString> texts;
    collectHeadings(markdown, lines, levels, texts);

    out << QString("共解析到 %1 个标题").arg(texts.size());
    QMap<QString, int> usedAnchors;
    for (int i = 0; i < texts.size(); ++i) {
        const QString base = DocModel::anchorBase(texts.at(i));
        const QString anchor = makeUniqueAnchor(base, usedAnchors);
        out << QString("  L%1 line=%2 anchor=%3 text=%4")
                   .arg(levels.at(i)).arg(lines.at(i)).arg(anchor).arg(texts.at(i));
    }
}

void OutlinePanel::updateOutline(const QString &markdown)
{
    QList<int> lines, levels;
    QList<QString> texts;
    collectHeadings(markdown, lines, levels, texts);

    // 保留当前展开状态与滚动位置
    //
    // 注意：不能用 `for (... it(m_tree); *it; ++it)` 的写法。
    // 树为空时 QTreeWidgetItemIterator 的 operator* 会解引用空指针导致崩溃
    // （首次打开文件、大纲为空时必崩）。
    // 正确写法是用 it != it.end() 判断。
    QList<int> expandedLevels;
    for (QTreeWidgetItemIterator it(m_tree); it.operator*() != nullptr; ++it) {
        if ((*it)->isExpanded()) {
            expandedLevels.append((*it)->data(0, Qt::UserRole).toInt());
        }
    }
    const int scrollPos = m_tree->verticalScrollBar()->value();

    m_tree->clear();
    m_itemLines.clear();
    m_itemAnchors.clear();

    if (texts.isEmpty()) {
        emit outlineChanged(0);
        return;
    }

    // 建立层级：维护每级标题的父节点栈
    QList<QTreeWidgetItem*> stack;      // stack[level-1] = 该层级的最后一个节点
    QMap<QString, int> usedAnchors;

    for (int i = 0; i < texts.size(); ++i) {
        const int level = levels.at(i);       // 1..6
        const QString text = texts.at(i);

        QTreeWidgetItem *item = new QTreeWidgetItem();
        item->setText(0, text);
        item->setData(0, Qt::UserRole, lines.at(i));
        item->setToolTip(0, tr("第 %1 行").arg(lines.at(i) + 1));

        // 生成唯一锚点（与 Typora 的 md-toc 锚点规则一致）
        const QString anchor = makeUniqueAnchor(DocModel::anchorBase(text), usedAnchors);
        item->setData(0, Qt::UserRole + 1, anchor);

        // 挂到正确的父节点
        QTreeWidgetItem *parent = nullptr;
        for (int l = level - 2; l >= 0; --l) {
            if (l < stack.size() && stack.at(l)) {
                parent = stack.at(l);
                break;
            }
        }
        if (parent) {
            parent->addChild(item);
        } else {
            m_tree->addTopLevelItem(item);
        }

        // 更新栈
        while (stack.size() >= level) {
            stack.removeLast();
        }
        stack.append(item);

        m_itemLines.insert(item, lines.at(i));
        m_itemAnchors.insert(item, anchor);
    }

    // 恢复展开状态
    for (QTreeWidgetItemIterator it(m_tree); it.operator*() != nullptr; ++it) {
        const int lv = (*it)->data(0, Qt::UserRole).toInt();
        // 默认展开前两层，其余折叠
        int depth = 0;
        for (QTreeWidgetItem *p = (*it)->parent(); p; p = p->parent()) {
            ++depth;
        }
        (*it)->setExpanded(expandedLevels.contains(lv) || depth < 1);
    }

    m_tree->verticalScrollBar()->setValue(scrollPos);

    emit outlineChanged(m_itemLines.size());
}

void OutlinePanel::setCurrentLine(int line)
{
    if (line == m_currentLine) {
        return;
    }
    m_currentLine = line;

    // 找到光标所在行所属的标题（向上最近的那个）
    QTreeWidgetItem *target = nullptr;
    for (QTreeWidgetItemIterator it(m_tree); it.operator*() != nullptr; ++it) {
        const int itemLine = (*it)->data(0, Qt::UserRole).toInt();
        if (itemLine <= line) {
            target = *it;
        } else {
            break;
        }
    }

    if (target) {
        m_tree->setCurrentItem(target);
        m_tree->scrollToItem(target, QAbstractItemView::EnsureVisible);
    }
}

void OutlinePanel::clear()
{
    m_tree->clear();
    m_itemLines.clear();
    m_itemAnchors.clear();
    m_currentLine = -1;
    emit outlineChanged(0);
}

void OutlinePanel::applyTheme(const QString &themeId)
{
    m_tree->setStyleSheet(Theme::outlineStyleSheet(themeId));
}

void OutlinePanel::onItemClicked(QTreeWidgetItem *item, int column)
{
    Q_UNUSED(column);
    if (!item) return;
    const int line = m_itemLines.value(item, -1);
    if (line >= 0) {
        emit navigateToLine(line);
    }
}

void OutlinePanel::onItemActivated(QTreeWidgetItem *item, int column)
{
    onItemClicked(item, column);
}

void OutlinePanel::keyPressEvent(QKeyEvent *event)
{
    // 上下键在树内移动，回车/空格触发跳转
    if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
        QTreeWidgetItem *item = m_tree->currentItem();
        if (item) {
            onItemClicked(item, 0);
            event->accept();
            return;
        }
    }
    QWidget::keyPressEvent(event);
}
