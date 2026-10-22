#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
把 Typora 风格的功能集成进 MainWindow：
  1. 大纲面板（Outline）—— 侧边栏 tab
  2. 主题系统 —— 菜单切换 + 预览区 CSS + 编辑器 QSS
  3. 智能标点 —— 菜单开关 + 编辑器输入拦截
  4. 源码模式 —— 切换纯源码视图
  5. 任务列表点击回写 —— 预览区 checkbox
  6. [TOC] 链接点击 —— 跳转到对应标题
"""
import io
import sys

SRC = sys.argv[1]

with io.open(SRC, 'r', encoding='utf-8') as f:
    src = f.read()

# ===========================================================================
# 1) 头文件：新增成员与槽
# ===========================================================================
HDR = SRC.replace('.cpp', '.h')
with io.open(HDR, 'r', encoding='utf-8') as f:
    hdr = f.read()

if 'outlinepanel.h' in hdr:
    print('  mainwindow.h 已包含新特性，跳过')
else:
    # 1.1 include
    hdr = hdr.replace(
        '#include "searchreplace.h"',
        '#include "searchreplace.h"\n'
        '#include "outlinepanel.h"\n'
        '#include "smartpunctuation.h"\n'
        '#include "theme.h"'
    )

    # 1.2 成员变量
    hdr = hdr.replace(
        '''    QTreeView *m_fileTree;
    QFileSystemModel *m_fileModel;
    QTextBrowser *m_preview;''',
        '''    QTreeView *m_fileTree;
    QFileSystemModel *m_fileModel;
    QTextBrowser *m_preview;

    // 大纲面板（对应 Typora 的 Outline）
    OutlinePanel *m_outlinePanel;
    QDockWidget *m_outlineDock;
    QDockWidget *m_fileTreeDock;
    QTabWidget *m_sidebarTabs;

    // 主题
    QString m_themeId;
    SmartPunctuation *m_smartPunctuation;
    bool m_sourceMode;'''
    )

    # 1.3 槽
    hdr = hdr.replace(
        '''    // 视图操作
    void togglePreview();''',
        '''    // 视图操作
    void togglePreview();
    void toggleOutline();
    void toggleSourceMode();
    void applyThemeById(const QString &themeId);
    void onOutlineNavigate(int line);
    void onPreviewAnchorClicked(const QUrl &url);
    void onTaskCheckboxClicked();
    void onEditorCursorMoved();
    void onSmartPunctuationToggled(bool enabled);'''
    )

    # 1.4 私有方法
    hdr = hdr.replace(
        '''    void applyEditorTheme(QTextEdit *editor);''',
        '''    void applyEditorTheme(QTextEdit *editor);
    void applyCurrentTheme();
    void refreshOutline();
    QTextEdit *editorAtLine(int line) const;'''
    )

    with io.open(HDR, 'w', encoding='utf-8', newline='\n') as f:
        f.write(hdr)
    print('  mainwindow.h 已更新')

# ===========================================================================
# 2) mainwindow.cpp
# ===========================================================================
with io.open(SRC, 'r', encoding='utf-8') as f:
    src = f.read()

# 2.1 构造：初始化新成员
src = src.replace(
    '''    , m_fileManager(new FileManager(this))
    , m_searchDialog(nullptr)
{''',
    '''    , m_fileManager(new FileManager(this))
    , m_searchDialog(nullptr)
    , m_outlinePanel(nullptr)
    , m_outlineDock(nullptr)
    , m_sidebarTabs(nullptr)
    , m_themeId(Theme::ThemeId::Default)
    , m_smartPunctuation(new SmartPunctuation(this))
    , m_sourceMode(false)
{'''
)

# 2.2 构造末尾：连接大纲导航
src = src.replace(
    '''    // 创建第一个空标签页
    newFile();
    
    setAcceptDrops(true);
}''',
    '''    // 创建第一个空标签页
    newFile();
    
    setAcceptDrops(true);
    
    // 大纲面板导航
    connect(m_outlinePanel, &OutlinePanel::navigateToLine,
            this, &MainWindow::onOutlineNavigate);
    
    // 预览区：处理 [TOC] 锚点点击与任务列表勾选
    connect(m_preview, &QTextBrowser::anchorClicked,
            this, &MainWindow::onPreviewAnchorClicked);
    
    // 智能标点：编辑器按键拦截
    if (QTextEdit *ed = currentEditor()) {
        ed->installEventFilter(this);
    }
    
    // 应用当前主题
    applyCurrentTheme();
}'''
)

# 2.3 createDockWidgets：加入大纲 tab
old_dock = '''void MainWindow::createDockWidgets()
{
    // 文件树Dock
    QDockWidget *fileTreeDock = new QDockWidget(tr("文件浏览器"), this);
    fileTreeDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    
    m_fileTree = new QTreeView(fileTreeDock);
    m_fileModel = new QFileSystemModel(this);
    m_fileTree->setModel(m_fileModel);
    m_fileTree->setHeaderHidden(true);
    m_fileTree->setAnimated(true);
    m_fileTree->setIndentation(20);
    
    connect(m_fileTree, &QTreeView::clicked, this, &MainWindow::onFileTreeClicked);
    connect(m_fileTree, &QTreeView::doubleClicked, this, &MainWindow::onFileTreeDoubleClicked);
    
    fileTreeDock->setWidget(m_fileTree);
    addDockWidget(Qt::LeftDockWidgetArea, fileTreeDock);
}'''

new_dock = '''void MainWindow::createDockWidgets()
{
    // 侧边栏：文件树 + 大纲 两个 tab
    QDockWidget *fileTreeDock = new QDockWidget(tr("侧边栏"), this);
    fileTreeDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    
    m_sidebarTabs = new QTabWidget(fileTreeDock);
    m_sidebarTabs->setDocumentMode(true);
    
    // 文件树
    m_fileTree = new QTreeView(m_sidebarTabs);
    m_fileModel = new QFileSystemModel(this);
    m_fileTree->setModel(m_fileModel);
    m_fileTree->setHeaderHidden(true);
    m_fileTree->setAnimated(true);
    m_fileTree->setIndentation(20);
    
    connect(m_fileTree, &QTreeView::clicked, this, &MainWindow::onFileTreeClicked);
    connect(m_fileTree, &QTreeView::doubleClicked, this, &MainWindow::onFileTreeDoubleClicked);
    
    m_sidebarTabs->addTab(m_fileTree, tr("文件树"));
    
    // 大纲面板（对应 Typora 的 Outline）
    m_outlinePanel = new OutlinePanel(m_sidebarTabs);
    m_sidebarTabs->addTab(m_outlinePanel, tr("大纲"));
    
    fileTreeDock->setWidget(m_sidebarTabs);
    addDockWidget(Qt::LeftDockWidgetArea, fileTreeDock);
    
    // 记录 dock 以便切换显隐
    m_fileTreeDock = fileTreeDock;
}'''

assert old_dock in src, 'createDockWidgets 未匹配'
src = src.replace(old_dock, new_dock, 1)

# 2.4 菜单：加主题菜单项与新功能入口
old_menu = '''    // 视图菜单
    QMenu *viewMenu = menuBar()->addMenu(tr("视图(&V)"));
    viewMenu->addAction(m_togglePreviewAction);
    viewMenu->addAction(m_toggleSidebarAction);
    viewMenu->addSeparator();
    viewMenu->addAction(m_darkModeAction);
    viewMenu->addAction(m_focusModeAction);
    viewMenu->addAction(m_fullscreenAction);
    viewMenu->addSeparator();
    viewMenu->addAction(m_zoomInAction);
    viewMenu->addAction(m_zoomOutAction);
    viewMenu->addAction(m_zoomResetAction);'''

new_menu = '''    // 视图菜单
    QMenu *viewMenu = menuBar()->addMenu(tr("视图(&V)"));
    viewMenu->addAction(m_togglePreviewAction);
    viewMenu->addAction(m_toggleSidebarAction);
    viewMenu->addAction(m_toggleOutlineAction);
    viewMenu->addAction(m_toggleSourceModeAction);
    viewMenu->addSeparator();
    viewMenu->addAction(m_darkModeAction);
    viewMenu->addAction(m_focusModeAction);
    viewMenu->addAction(m_fullscreenAction);
    viewMenu->addSeparator();
    viewMenu->addAction(m_zoomInAction);
    viewMenu->addAction(m_zoomOutAction);
    viewMenu->addAction(m_zoomResetAction);
    viewMenu->addSeparator();
    
    // 主题菜单（对应 Typora 的「视图 > 主题」）
    QMenu *themeMenu = viewMenu->addMenu(tr("主题(&T)"));
    themeMenu->setObjectName("themeMenu");
    {
        QActionGroup *group = new QActionGroup(this);
        group->setExclusive(true);
        for (const QString &tid : Theme::availableThemes()) {
            const Theme::Palette pal = Theme::palette(tid);
            QAction *act = new QAction(pal.displayName, this);
            act->setCheckable(true);
            act->setData(tid);
            act->setChecked(tid == m_themeId);
            connect(act, &QAction::triggered, this, [this, tid]() { applyThemeById(tid); });
            group->addAction(act);
            themeMenu->addAction(act);
        }
    }
    
    // 编辑菜单：智能标点
    editMenu->addSeparator();
    editMenu->addAction(m_smartPunctuationAction);'''

assert old_menu in src, '视图菜单未匹配'
src = src.replace(old_menu, new_menu, 1)

# 2.5 createActions：新增 action
old_act = '''    m_zoomResetAction = new QAction(QIcon(":/icons/zoomreset.svg"), tr("重置缩放"), this);
    m_zoomResetAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_0));
    connect(m_zoomResetAction, &QAction::triggered, this, [this]() { updateFontSize(14); });'''

new_act = '''    m_zoomResetAction = new QAction(tr("重置缩放"), this);
    m_zoomResetAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_0));
    connect(m_zoomResetAction, &QAction::triggered, this, [this]() { updateFontSize(14); });
    
    // 大纲面板（对应 Typora 的 Outline）
    m_toggleOutlineAction = new QAction(QIcon(":/icons/outline.svg"), tr("显示/隐藏大纲"), this);
    m_toggleOutlineAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_O));
    m_toggleOutlineAction->setCheckable(true);
    m_toggleOutlineAction->setChecked(true);
    connect(m_toggleOutlineAction, &QAction::triggered, this, &MainWindow::toggleOutline);
    
    // 源码模式（对应 Typora 的 Source Code Mode）
    m_toggleSourceModeAction = new QAction(QIcon(":/icons/codemode.svg"), tr("源码模式"), this);
    m_toggleSourceModeAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_M));
    m_toggleSourceModeAction->setCheckable(true);
    connect(m_toggleSourceModeAction, &QAction::triggered, this, &MainWindow::toggleSourceMode);
    
    // 智能标点（对应 Typora 的 Smart Quotes / Smart Dashes）
    m_smartPunctuationAction = new QAction(tr("智能标点"), this);
    m_smartPunctuationAction->setCheckable(true);
    m_smartPunctuationAction->setChecked(true);
    connect(m_smartPunctuationAction, &QAction::triggered, this, &MainWindow::onSmartPunctuationToggled);'''

assert old_act in src, 'createActions 尾部未匹配'
src = src.replace(old_act, new_act, 1)

# 2.6 工具栏：大纲按钮
old_tb = '''    // 视图工具栏
    QToolBar *viewToolbar = addToolBar(tr("视图"));
    viewToolbar->setIconSize(QSize(24, 24));
    viewToolbar->addAction(m_togglePreviewAction);
    viewToolbar->addAction(m_toggleSidebarAction);'''

new_tb = '''    // 视图工具栏
    QToolBar *viewToolbar = addToolBar(tr("视图"));
    viewToolbar->setIconSize(QSize(24, 24));
    viewToolbar->addAction(m_togglePreviewAction);
    viewToolbar->addAction(m_toggleSidebarAction);
    viewToolbar->addAction(m_toggleOutlineAction);
    viewToolbar->addAction(m_toggleSourceModeAction);'''

assert old_tb in src, '视图工具栏未匹配'
src = src.replace(old_tb, new_tb, 1)

with io.open(SRC, 'w', encoding='utf-8', newline='\n') as f:
    f.write(src)
print('  mainwindow.cpp 已更新（阶段 1）')
