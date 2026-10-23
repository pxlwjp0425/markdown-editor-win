#include "mainwindow.h"
#include "appicon.h"
#include "mergedialog.h"
#include <QApplication>
#include <QClipboard>
#include <QScrollBar>
#include <QStandardPaths>
#include <QFormLayout>
#include <QVBoxLayout>
#include <QSpinBox>
#include <QComboBox>
#include <QCheckBox>
#include <QDialogButtonBox>
#include <QMenu>
#include <QActionGroup>
#include <QDir>
#include <QRegularExpression>
#include <QStringConverter>
#include <QRegularExpression>

#include <QFile>
#include <QFileDialog>
#include <QMessageBox>
#include <QFont>
#include <QFontDialog>
#include <QColorDialog>
#include <QPrintDialog>
#include <QPrinter>
#include <QPainter>
#include <QAbstractTextDocumentLayout>
#include <QPageSize>
#include <QPageLayout>
#include <QTextDocumentWriter>
#include <QImage>
#include <QPageSetupDialog>
#include <QDateTime>
#include <QSettings>
#include <QMimeData>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QDesktopServices>
#include <QUrl>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    // 初始化顺序必须与 mainwindow.h 中的成员声明顺序一致，
    // 否则 GCC 的 -Wreorder 会报警告（实际也属于未定义行为：
    // 用未初始化的成员去初始化另一个成员）。
    , m_mainSplitter(nullptr)
    , m_editorTabs(nullptr)
    , m_fileTree(nullptr)
    , m_fileModel(nullptr)
    , m_preview(nullptr)
    , m_outlinePanel(nullptr)
    , m_outlineDock(nullptr)
    , m_fileTreeDock(nullptr)
    , m_sidebarTabs(nullptr)
    , m_themeId(Theme::ThemeId::Default)
    , m_smartPunctuation(new SmartPunctuation(this))
    , m_sourceMode(false)
    , m_headlessMode(false)
    , m_fileManager(new FileManager(this))
    , m_searchDialog(nullptr)
    , m_previewVisible(true)
    , m_sidebarVisible(true)
    , m_darkMode(false)
    , m_focusMode(false)
    , m_previewOnlyMode(false)
    , m_fontSize(14)
    , m_inlineFormatter(new InlineFormatter())
    , m_blockEditor(new BlockEditor())
    , m_benchMode(false)
    , m_typewriterMode(false)
    , m_sidebarTabIndex(0)
    , m_docTree(nullptr)
    , m_autoSaveTimer(nullptr)
    , m_previewUpdateTimer(nullptr)
    , m_statusUpdateTimer(nullptr)
    , m_outlineTimer(nullptr)
{
    setupUi();
    createActions();
    createMenus();
    createToolbars();
    createStatusBar();
    createDockWidgets();
    createSearchReplaceDialog();
    
    loadSettings();
    
    // 设置计时器
    m_autoSaveTimer = new QTimer(this);
    connect(m_autoSaveTimer, &QTimer::timeout, this, &MainWindow::onAutoSaveTimer);
    m_autoSaveTimer->start(30000); // 30秒自动保存
    
    m_previewUpdateTimer = new QTimer(this);
    m_previewUpdateTimer->setSingleShot(true);
    connect(m_previewUpdateTimer, &QTimer::timeout, this, &MainWindow::updatePreview);
    
    m_statusUpdateTimer = new QTimer(this);
    connect(m_statusUpdateTimer, &QTimer::timeout, this, &MainWindow::updateStatusBar);
    m_statusUpdateTimer->start(1000);
    
    // 大纲刷新计时器（单次触发，防抖）
    // 必须在构造函数中创建：若在 onTextChanged 里懒创建，
    // 会在 setText() 的信号发射过程中 new 对象，容易触发异常。
    m_outlineTimer = new QTimer(this);
    m_outlineTimer->setSingleShot(true);
    connect(m_outlineTimer, &QTimer::timeout, this, &MainWindow::refreshOutline);
    
    // 创建第一个空标签页
    newFile();
    
    setAcceptDrops(true);
    
    // 大纲面板导航
    connect(m_outlinePanel, &OutlinePanel::navigateToLine,
            this, &MainWindow::onOutlineNavigate);
    // 大纲点击同时滚动预览到对应标题（解决「点击大纲不跳转 / 预览专属模式看不到跳转」）
    connect(m_outlinePanel, &OutlinePanel::navigateToAnchor,
            this, &MainWindow::onOutlineAnchorNavigate);
    
    // 预览区：处理 [TOC] 锚点点击与任务列表勾选
    connect(m_preview, &QTextBrowser::anchorClicked,
            this, &MainWindow::onPreviewAnchorClicked);
    
    // 智能标点：编辑器按键拦截
    if (QPlainTextEdit *ed = currentEditor()) {
        ed->installEventFilter(this);
    }
    
    // 应用当前主题
    applyCurrentTheme();
}

MainWindow::~MainWindow()
{
    saveSettings();

    // 这两个是纯 C++ 类（非 QObject），没有父对象可依附，
    // Qt 的父子回收机制管不到，必须显式释放。
    delete m_inlineFormatter;
    delete m_blockEditor;
}

void MainWindow::setupUi()
{
    setWindowTitle("Markdown Editor");
    setWindowIcon(makeAppIcon());
    resize(1200, 800);
    
    // 创建主分割器
    m_mainSplitter = new QSplitter(Qt::Horizontal, this);
    setCentralWidget(m_mainSplitter);
    
    // 创建编辑器标签页
    m_editorTabs = new QTabWidget(this);
    m_editorTabs->setTabsClosable(true);
    m_editorTabs->setMovable(true);
    m_editorTabs->setDocumentMode(true);
    connect(m_editorTabs, &QTabWidget::currentChanged, this, &MainWindow::onTabChanged);
    connect(m_editorTabs, &QTabWidget::tabCloseRequested, this, &MainWindow::closeTab);
    
    // 创建预览视图
    m_preview = new QTextBrowser(this);
    m_preview->setMinimumWidth(300);
    m_preview->setOpenExternalLinks(true);
    
    // 预览区自身尺寸变化时也要重算居中边距
    // （分割条拖动不会触发 MainWindow 的 resizeEvent）
    m_preview->installEventFilter(this);

    // 预览标签栏：仅在「单独预览模式」显示，用于在只有预览时切换已打开的文件。
    // 它与编辑器标签（m_editorTabs）双向同步 —— 点它即切换编辑器当前标签，
    // 预览随之更新；编辑器标签切换时它也跟随高亮。
    m_previewTabs = new QTabBar(this);
    m_previewTabs->setVisible(false);
    m_previewTabs->setExpanding(false);
    m_previewTabs->setDrawBase(false);
    connect(m_previewTabs, &QTabBar::currentChanged,
            this, &MainWindow::onPreviewTabChanged);

    // 用容器把「预览标签栏 + 预览区」竖直排列，再把容器放进主分割器。
    QWidget *previewWrap = new QWidget(this);
    auto *previewLayout = new QVBoxLayout(previewWrap);
    previewLayout->setContentsMargins(0, 0, 0, 0);
    previewLayout->setSpacing(0);
    previewLayout->addWidget(m_previewTabs);
    previewLayout->addWidget(m_preview, 1);
    
    // 添加到主分割器
    m_mainSplitter->addWidget(m_editorTabs);
    m_mainSplitter->addWidget(previewWrap);
    m_mainSplitter->setStretchFactor(0, 1);
    m_mainSplitter->setStretchFactor(1, 1);

    // QPlainTextEdit 的 sizeHint 比 QTextEdit 小得多（它按内容行数估，
    // 而不是像 QTextEdit 那样给一个固定的富文本默认尺寸）。
    // 如果不给下限，QSplitter 首次布局会把编辑器压成很窄的一条 ——
    // 表现为源码区只显示几个字就换行。
    m_editorTabs->setMinimumWidth(360);
    m_mainSplitter->setSizes(QList<int>() << 1 << 1);

    // QTextBrowser 不需要 WebEngine 设置
}

void MainWindow::createActions()
{
    // 文件操作
    m_newAction = new QAction(QIcon(":/icons/new.svg"), tr("新建"), this);
    m_newAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_N));
    m_newAction->setStatusTip(tr("创建新文件"));
    connect(m_newAction, &QAction::triggered, this, &MainWindow::newFile);
    
    m_openAction = new QAction(QIcon(":/icons/open.svg"), tr("打开"), this);
    m_openAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_O));
    m_openAction->setStatusTip(tr("打开文件"));
    connect(m_openAction, SIGNAL(triggered()), this, SLOT(openFile()));
    
    m_openFolderAction = new QAction(QIcon(":/icons/folder.svg"), tr("打开文件夹"), this);
    // 原为 Ctrl+Shift+O（与有序列表冲突），Ctrl+Shift+F（与专注模式冲突）；
    // 改用 Ctrl+Shift+L
    m_openFolderAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_O));
    m_openFolderAction->setStatusTip(tr("打开文件夹"));
    connect(m_openFolderAction, &QAction::triggered, this, &MainWindow::openFolder);

    m_mergeAction = new QAction(QIcon(":/icons/merge.svg"), tr("合并 Markdown..."), this);
    m_mergeAction->setStatusTip(tr("将多个 Markdown 文件合并为一个"));
    connect(m_mergeAction, &QAction::triggered, this, &MainWindow::mergeMarkdown);
    
    m_saveAction = new QAction(QIcon(":/icons/save.svg"), tr("保存"), this);
    m_saveAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_S));
    m_saveAction->setStatusTip(tr("保存文件"));
    connect(m_saveAction, &QAction::triggered, this, &MainWindow::saveFile);
    
    m_saveAsAction = new QAction(QIcon(":/icons/save.svg"), tr("另存为..."), this);
    m_saveAsAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_S));
    m_saveAsAction->setStatusTip(tr("另存为"));
    connect(m_saveAsAction, &QAction::triggered, this, &MainWindow::saveFileAs);

    // 关闭当前标签页。
    // 原来是写在 keyPressEvent 里的裸 switch，但焦点在编辑器时按键事件会被
    // QPlainTextEdit 消费，根本冒泡不到 MainWindow —— Ctrl+W 实际上时灵时不灵。
    // 改成 QAction 后由 QShortcutMap 统一分发，与焦点位置无关。
    m_closeTabAction = new QAction(tr("关闭标签页"), this);
    m_closeTabAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_W));
    m_closeTabAction->setStatusTip(tr("关闭当前标签页"));
    connect(m_closeTabAction, &QAction::triggered, this, [this]() {
        closeTab(m_editorTabs->currentIndex());
    });

    m_exportHtmlAction = new QAction(QIcon(":/icons/exporthtml.svg"), tr("导出为HTML..."), this);
    m_exportHtmlAction->setStatusTip(tr("导出为HTML"));
    connect(m_exportHtmlAction, &QAction::triggered, this, &MainWindow::exportToHtml);
    
    m_exportPdfAction = new QAction(QIcon(":/icons/exportpdf.svg"), tr("导出为PDF..."), this);
    m_exportPdfAction->setStatusTip(tr("导出为PDF"));
    connect(m_exportPdfAction, &QAction::triggered, this, &MainWindow::exportToPdf);
    
    m_exitAction = new QAction(tr("退出"), this);
    m_exitAction->setShortcut(QKeySequence::Quit);
    connect(m_exitAction, &QAction::triggered, this, &QWidget::close);
    
    // 编辑操作
    m_undoAction = new QAction(QIcon(":/icons/undo.svg"), tr("撤销"), this);
    m_undoAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_Z));
    connect(m_undoAction, &QAction::triggered, this, &MainWindow::undo);
    
    m_redoAction = new QAction(QIcon(":/icons/redo.svg"), tr("重做"), this);
    m_redoAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_Y));
    connect(m_redoAction, &QAction::triggered, this, &MainWindow::redo);
    
    m_cutAction = new QAction(QIcon(":/icons/cut.svg"), tr("剪切"), this);
    m_cutAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_X));
    connect(m_cutAction, &QAction::triggered, this, &MainWindow::cut);
    
    m_copyAction = new QAction(QIcon(":/icons/copy.svg"), tr("复制"), this);
    m_copyAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_C));
    connect(m_copyAction, &QAction::triggered, this, &MainWindow::copy);
    
    m_pasteAction = new QAction(QIcon(":/icons/paste.svg"), tr("粘贴"), this);
    m_pasteAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_V));
    connect(m_pasteAction, &QAction::triggered, this, &MainWindow::paste);
    
    m_findAction = new QAction(QIcon(":/icons/find.svg"), tr("查找"), this);
    m_findAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_F));
    connect(m_findAction, &QAction::triggered, this, &MainWindow::find);
    
    m_replaceAction = new QAction(QIcon(":/icons/replace.svg"), tr("查找和替换"), this);
    // 原用 QKeySequence::Find（Ctrl+F），与「查找」完全冲突；改为 Ctrl+R
    m_replaceAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_R));
    connect(m_replaceAction, &QAction::triggered, this, &MainWindow::replace);
    
    // 格式操作
    m_boldAction = new QAction(QIcon(":/icons/bold.svg"), tr("粗体"), this);
    m_boldAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_B));
    connect(m_boldAction, &QAction::triggered, this, &MainWindow::insertBold);
    
    m_italicAction = new QAction(QIcon(":/icons/italic.svg"), tr("斜体"), this);
    m_italicAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_I));
    connect(m_italicAction, &QAction::triggered, this, &MainWindow::insertItalic);
    
    m_strikethroughAction = new QAction(QIcon(":/icons/strikethrough.svg"), tr("删除线"), this);
    // 原为 Ctrl+Shift+S，与「另存为」冲突；改为 Ctrl+Shift+X
    m_strikethroughAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_K));
    connect(m_strikethroughAction, &QAction::triggered, this, &MainWindow::insertStrikethrough);
    
    m_heading1Action = new QAction(QIcon(":/icons/heading1.svg"), tr("标题1"), this);
    m_heading1Action->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_1));
    connect(m_heading1Action, &QAction::triggered, this, &MainWindow::insertHeading1);
    
    m_heading2Action = new QAction(QIcon(":/icons/heading2.svg"), tr("标题2"), this);
    m_heading2Action->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_2));
    connect(m_heading2Action, &QAction::triggered, this, &MainWindow::insertHeading2);
    
    m_heading3Action = new QAction(QIcon(":/icons/heading3.svg"), tr("标题3"), this);
    m_heading3Action->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_3));
    connect(m_heading3Action, &QAction::triggered, this, &MainWindow::insertHeading3);
    
    m_codeAction = new QAction(QIcon(":/icons/code.svg"), tr("行内代码"), this);
    m_codeAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_E));
    connect(m_codeAction, &QAction::triggered, this, &MainWindow::insertCode);
    
    m_codeBlockAction = new QAction(QIcon(":/icons/codeblock.svg"), tr("代码块"), this);
    m_codeBlockAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_C));
    connect(m_codeBlockAction, &QAction::triggered, this, &MainWindow::insertCodeBlock);
    
    m_linkAction = new QAction(QIcon(":/icons/link.svg"), tr("链接"), this);
    m_linkAction->setShortcut(QKeySequence(Qt::ALT | Qt::Key_L));
    connect(m_linkAction, &QAction::triggered, this, &MainWindow::insertLink);
    
    m_imageAction = new QAction(QIcon(":/icons/image.svg"), tr("图片"), this);
    m_imageAction->setShortcut(QKeySequence(Qt::ALT | Qt::Key_I));
    connect(m_imageAction, &QAction::triggered, this, &MainWindow::insertImage);
    
    m_tableAction = new QAction(QIcon(":/icons/table.svg"), tr("表格"), this);
    m_tableAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_T));
    connect(m_tableAction, &QAction::triggered, this, &MainWindow::insertTable);
    
    m_bulletListAction = new QAction(QIcon(":/icons/bulletlist.svg"), tr("无序列表"), this);
    m_bulletListAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_L));
    connect(m_bulletListAction, &QAction::triggered, this, &MainWindow::insertListBullet);
    
    m_numberedListAction = new QAction(QIcon(":/icons/numberedlist.svg"), tr("有序列表"), this);
    m_numberedListAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_U));
    connect(m_numberedListAction, &QAction::triggered, this, &MainWindow::insertListNumbered);
    
    m_blockquoteAction = new QAction(QIcon(":/icons/blockquote.svg"), tr("引用"), this);
    // 原为 Ctrl+Q，与「退出」冲突；改为 Ctrl+Shift+Q
    m_blockquoteAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_Q));
    connect(m_blockquoteAction, &QAction::triggered, this, &MainWindow::insertBlockquote);
    
    m_horizontalRuleAction = new QAction(QIcon(":/icons/horizontalrule.svg"), tr("水平线"), this);
    m_horizontalRuleAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_M));
    connect(m_horizontalRuleAction, &QAction::triggered, this, &MainWindow::insertHorizontalRule);
    
    // 视图操作
    m_togglePreviewAction = new QAction(QIcon(":/icons/preview.svg"), tr("显示/隐藏预览"), this);
    m_togglePreviewAction->setShortcut(QKeySequence(Qt::Key_F4));
    m_togglePreviewAction->setCheckable(true);
    m_togglePreviewAction->setChecked(true);
    connect(m_togglePreviewAction, &QAction::triggered, this, &MainWindow::togglePreview);
    
    m_toggleSidebarAction = new QAction(QIcon(":/icons/sidebar.svg"), tr("显示/隐藏侧边栏"), this);
    m_toggleSidebarAction->setShortcut(QKeySequence(Qt::ALT | Qt::Key_B));
    // 原为 Ctrl+B，与「粗体」冲突；改为 Ctrl+Shift+B
    m_toggleSidebarAction->setCheckable(true);
    m_toggleSidebarAction->setChecked(true);
    connect(m_toggleSidebarAction, &QAction::triggered, this, &MainWindow::toggleSidebar);
    
    m_fullscreenAction = new QAction(QIcon(":/icons/fullscreen.svg"), tr("全屏"), this);
    m_fullscreenAction->setShortcut(QKeySequence(Qt::Key_F5));
    connect(m_fullscreenAction, &QAction::triggered, this, &MainWindow::toggleFullscreen);
    
    m_darkModeAction = new QAction(QIcon(":/icons/darkmode.svg"), tr("深色模式"), this);
    m_darkModeAction->setShortcut(QKeySequence(Qt::ALT | Qt::Key_D));
    m_darkModeAction->setCheckable(true);
    connect(m_darkModeAction, &QAction::triggered, this, [this](bool checked) { setDarkMode(checked); });
    
    m_focusModeAction = new QAction(QIcon(":/icons/focusmode.svg"), tr("专注模式"), this);
    m_focusModeAction->setShortcut(QKeySequence(Qt::Key_F9));
    m_focusModeAction->setCheckable(true);
    connect(m_focusModeAction, &QAction::triggered, this, [this](bool checked) { setFocusMode(checked); });
    
    m_zoomInAction = new QAction(QIcon(":/icons/zoomin.svg"), tr("放大"), this);
    m_zoomInAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_Equal));
    connect(m_zoomInAction, &QAction::triggered, this, [this]() { updateFontSize(m_fontSize + 2); });
    
    m_zoomOutAction = new QAction(QIcon(":/icons/zoomout.svg"), tr("缩小"), this);
    m_zoomOutAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_Minus));
    connect(m_zoomOutAction, &QAction::triggered, this, [this]() { updateFontSize(m_fontSize - 2); });
    
    m_zoomResetAction = new QAction(tr("重置缩放"), this);
    m_zoomResetAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_0));
    connect(m_zoomResetAction, &QAction::triggered, this, [this]() { updateFontSize(14); });
    
    // 大纲面板（对应 Typora 的 Outline）
    m_toggleOutlineAction = new QAction(QIcon(":/icons/outline.svg"), tr("显示/隐藏大纲"), this);
    m_toggleOutlineAction->setShortcut(QKeySequence(Qt::ALT | Qt::Key_O));
    // Ctrl+Shift+O 已被「有序列表」占用，此处用 Ctrl+Shift+N
    m_toggleOutlineAction->setCheckable(true);
    m_toggleOutlineAction->setChecked(true);
    connect(m_toggleOutlineAction, &QAction::triggered, this, &MainWindow::toggleOutline);
    
    // 单独预览模式：只显示预览区，隐藏编辑器与侧边栏
    // Ctrl+Shift+P 与「打印」不冲突，Ctrl+Shift+R 已被「查找替换」占用
    m_previewOnlyAction = new QAction(QIcon(":/icons/previewonly.svg"), tr("单独预览模式"), this);
    m_previewOnlyAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_P));
    m_previewOnlyAction->setCheckable(true);
    m_previewOnlyAction->setToolTip(tr("只显示预览区，隐藏编辑器与侧边栏（专注阅读）"));
    connect(m_previewOnlyAction, &QAction::triggered, this, &MainWindow::setPreviewOnlyMode);

    // ================= Typora 对齐：文件菜单 =================
    m_newWindowAction = new QAction(tr("新建窗口"), this);
    m_newWindowAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_N));
    connect(m_newWindowAction, &QAction::triggered, this, [this]() {
        MainWindow *w = new MainWindow();
        w->resize(1000, 700);
        w->show();
    });

    m_printAction = new QAction(tr("打印..."), this);
    m_printAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_P));
    connect(m_printAction, &QAction::triggered, this, &MainWindow::showPrintDialog);

    m_preferencesAction = new QAction(tr("偏好设置..."), this);
    m_preferencesAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_Comma));
    connect(m_preferencesAction, &QAction::triggered, this, &MainWindow::showPreferences);

    m_preferencesFileAction = new QAction(tr("偏好设置文件..."), this);
    connect(m_preferencesFileAction, &QAction::triggered, this, [this]() {
        const QString path = QDir(QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation))
                                .filePath("preferences.json");
        QDesktopServices::openUrl(QUrl::fromLocalFile(path));
    });

    // ================= Typora 对齐：编辑菜单 =================
    m_pasteMarkdownAction = new QAction(tr("粘贴为 Markdown"), this);
    m_pasteMarkdownAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_V));
    connect(m_pasteMarkdownAction, &QAction::triggered, this, &MainWindow::pasteAsMarkdown);

    m_selectAllAction = new QAction(tr("全选"), this);
    m_selectAllAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_A));
    connect(m_selectAllAction, &QAction::triggered, this, &MainWindow::selectAll);

    m_copyMarkdownAction = new QAction(tr("复制为 Markdown"), this);
    m_copyMarkdownAction->setShortcut(QKeySequence(Qt::ALT | Qt::Key_M));
    connect(m_copyMarkdownAction, &QAction::triggered, this, &MainWindow::copyAsMarkdown);

    m_copyHtmlAction = new QAction(tr("复制为 HTML 代码"), this);
    m_copyHtmlAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_H));
    connect(m_copyHtmlAction, &QAction::triggered, this, &MainWindow::copyAsHtml);

    m_pastePlainTextAction = new QAction(tr("粘贴为纯文本"), this);
    m_pastePlainTextAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_T));
    connect(m_pastePlainTextAction, &QAction::triggered, this, &MainWindow::pasteAsPlainText);

    m_deleteAction = new QAction(tr("删除"), this);
    m_deleteAction->setShortcut(QKeySequence(Qt::Key_Delete));
    connect(m_deleteAction, &QAction::triggered, this, &MainWindow::deleteSelection);

    m_mathToolsAction = new QAction(tr("数学工具"), this);
    m_mathToolsAction->setMenu(new QMenu(this));
    m_mathToolsAction->menu()->addAction(tr("公式块..."));
    m_mathToolsAction->menu()->addAction(tr("内嵌公式..."));
    m_mathToolsAction->menu()->addAction(tr("刷新所有数学公式"));

    // 智能标点子项（Typora「编辑 > 智能标点」为子菜单）
    m_smartQuotesAction = new QAction(tr("智能引号"), this);
    m_smartQuotesAction->setCheckable(true);
    m_smartQuotesAction->setChecked(true);
    connect(m_smartQuotesAction, &QAction::toggled, this, [this](bool on) {
        m_smartPunctuation->setSmartQuotes(on);
    });

    m_smartDashesAction = new QAction(tr("智能破折号"), this);
    m_smartDashesAction->setCheckable(true);
    m_smartDashesAction->setChecked(true);
    connect(m_smartDashesAction, &QAction::toggled, this, [this](bool on) {
        m_smartPunctuation->setSmartDashes(on);
    });

    m_unicodePunctuationAction = new QAction(tr("解析 Markdown 时允许并转化 Unicode 标点"), this);
    m_unicodePunctuationAction->setCheckable(true);
    m_unicodePunctuationAction->setChecked(false);

    // ================= Typora 对齐：段落菜单 =================
    m_heading4Action = new QAction(tr("四级标题"), this);
    m_heading4Action->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_4));
    connect(m_heading4Action, &QAction::triggered, this, [this]() { insertHeading(4); });

    m_heading5Action = new QAction(tr("五级标题"), this);
    m_heading5Action->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_5));
    connect(m_heading5Action, &QAction::triggered, this, [this]() { insertHeading(5); });

    m_heading6Action = new QAction(tr("六级标题"), this);
    m_heading6Action->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_6));
    connect(m_heading6Action, &QAction::triggered, this, [this]() { insertHeading(6); });

    m_paragraphAction = new QAction(tr("段落"), this);
    m_paragraphAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_0));
    connect(m_paragraphAction, &QAction::triggered, this, [this]() { toggleBlockType("paragraph"); });

    m_increaseHeadingAction = new QAction(tr("提升标题级别"), this);
    m_increaseHeadingAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_BracketLeft));
    connect(m_increaseHeadingAction, &QAction::triggered, this, [this]() {
        applyBlockResult(BlockEditor::increaseHeading(currentText(), currentCursorPos()));
    });

    m_decreaseHeadingAction = new QAction(tr("降低标题级别"), this);
    m_decreaseHeadingAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_BracketRight));
    connect(m_decreaseHeadingAction, &QAction::triggered, this, [this]() {
        applyBlockResult(BlockEditor::decreaseHeading(currentText(), currentCursorPos()));
    });

    m_tableBlockAction = new QAction(tr("表格"), this);
    m_tableBlockAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_BracketRight));
    connect(m_tableBlockAction, &QAction::triggered, this, &MainWindow::insertTable);

    m_mathBlockAction = new QAction(tr("公式块"), this);
    m_mathBlockAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_M));
    connect(m_mathBlockAction, &QAction::triggered, this, [this]() { toggleBlockType("math"); });

    m_inlineCodeAction = new QAction(tr("代码"), this);
    m_inlineCodeAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_I));
    connect(m_inlineCodeAction, &QAction::triggered, this, [this]() { toggleInlineFormat("code"); });

    m_taskListAction = new QAction(tr("任务列表"), this);
    m_taskListAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_X));
    connect(m_taskListAction, &QAction::triggered, this, [this]() { toggleBlockType("task"); });

    m_taskStatusAction = new QAction(tr("任务状态"), this);
    m_taskStatusAction->setMenu(new QMenu(this));
    m_taskStatusAction->menu()->addAction(tr("切换任务状态"));
    m_taskStatusAction->menu()->addAction(tr("标记为已完成"));
    m_taskStatusAction->menu()->addAction(tr("标记为未完成"));

    m_indentAction = new QAction(tr("增加缩进"), this);
    m_indentAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_Right));
    connect(m_indentAction, &QAction::triggered, this, &MainWindow::indentList);

    m_outdentAction = new QAction(tr("减少缩进"), this);
    m_outdentAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_Left));
    connect(m_outdentAction, &QAction::triggered, this, &MainWindow::outdentList);

    m_tableRowAction = new QAction(tr("表格行"), this);
    m_tableRowAction->setMenu(new QMenu(this));
    m_tableRowAction->menu()->addAction(tr("上方插入行"));
    m_tableRowAction->menu()->addAction(tr("下方插入行"));
    m_tableRowAction->menu()->addAction(tr("删除行"));

    m_tableColumnAction = new QAction(tr("表格列"), this);
    m_tableColumnAction->setMenu(new QMenu(this));
    m_tableColumnAction->menu()->addAction(tr("左侧插入列"));
    m_tableColumnAction->menu()->addAction(tr("右侧插入列"));
    m_tableColumnAction->menu()->addAction(tr("删除列"));

    m_insertParagraphAboveAction = new QAction(tr("在上方插入段落"), this);
    connect(m_insertParagraphAboveAction, &QAction::triggered, this, &MainWindow::insertParagraphAbove);

    m_insertParagraphBelowAction = new QAction(tr("在下方插入段落"), this);
    connect(m_insertParagraphBelowAction, &QAction::triggered, this, &MainWindow::insertParagraphBelow);

    m_inlineMathAction = new QAction(tr("数学公式"), this);
    connect(m_inlineMathAction, &QAction::triggered, this, &MainWindow::insertInlineMath);

    m_footnoteAction = new QAction(tr("脚注"), this);
    connect(m_footnoteAction, &QAction::triggered, this, &MainWindow::insertFootnote);

    m_tocAction = new QAction(tr("内容目录"), this);
    connect(m_tocAction, &QAction::triggered, this, &MainWindow::insertToc);

    m_yamlFrontMatterAction = new QAction(tr("YAML Front Matter"), this);
    connect(m_yamlFrontMatterAction, &QAction::triggered, this, &MainWindow::insertYamlFrontMatter);

    // ================= Typora 对齐：格式菜单 =================
    m_highlightAction = new QAction(tr("高亮"), this);
    m_highlightAction->setShortcut(QKeySequence(Qt::ALT | Qt::SHIFT | Qt::Key_H));
    connect(m_highlightAction, &QAction::triggered, this, [this]() { toggleInlineFormat("highlight"); });

    m_superscriptAction = new QAction(tr("上标"), this);
    m_superscriptAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_Plus));
    connect(m_superscriptAction, &QAction::triggered, this, [this]() { toggleInlineFormat("superscript"); });

    m_subscriptAction = new QAction(tr("下标"), this);
    m_subscriptAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_Underscore));
    connect(m_subscriptAction, &QAction::triggered, this, [this]() { toggleInlineFormat("subscript"); });

    m_commentAction = new QAction(tr("注释"), this);
    connect(m_commentAction, &QAction::triggered, this, [this]() { toggleInlineFormat("comment"); });

    m_hyperlinkAction = new QAction(tr("超链接"), this);
    m_hyperlinkAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_K));
    connect(m_hyperlinkAction, &QAction::triggered, this, [this]() { toggleInlineFormat("link"); });

    m_createFootnoteAction = new QAction(tr("创建脚注"), this);
    connect(m_createFootnoteAction, &QAction::triggered, this, &MainWindow::insertFootnote);

    m_clearFormatAction = new QAction(tr("清除格式"), this);
    connect(m_clearFormatAction, &QAction::triggered, this, &MainWindow::clearFormat);

    // ================= Typora 对齐：视图菜单 =================
    m_outlineAction = new QAction(tr("大纲"), this);
    m_outlineAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_1));
    // Ctrl+Shift+1 作为大纲开关的快捷方式：按当前勾选态翻转。
    // （真正独立的勾选开关是 m_toggleOutlineAction，见下方。）
    connect(m_outlineAction, &QAction::triggered, this, [this]() {
        toggleOutline(!m_toggleOutlineAction->isChecked());
    });

    m_docTreeAction = new QAction(tr("文档树"), this);
    m_docTreeAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_2));
    connect(m_docTreeAction, &QAction::triggered, this, [this]() { setSidebarTab(2); });

    m_fileTreeAction = new QAction(tr("文件树"), this);
    m_fileTreeAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_3));
    connect(m_fileTreeAction, &QAction::triggered, this, [this]() { setSidebarTab(0); });

    m_sourceModeAction = new QAction(tr("源代码模式"), this);
    m_sourceModeAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_Slash));
    m_sourceModeAction->setCheckable(true);
    connect(m_sourceModeAction, &QAction::triggered, this, &MainWindow::toggleSourceMode);

    m_typewriterModeAction = new QAction(tr("打字机模式"), this);
    m_typewriterModeAction->setShortcut(QKeySequence(Qt::Key_F10));
    m_typewriterModeAction->setCheckable(true);
    connect(m_typewriterModeAction, &QAction::triggered, this, &MainWindow::setTypewriterMode);

    m_showSourceAction = new QAction(tr("显示源代码"), this);
    m_showSourceAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_D));
    // 「显示源代码」与「源代码模式」是同一个开关。
    // 早期实现是 ed->setPlainText(ed->toPlainText()) —— 内容没变，
    // 但白白清空了撤销栈、丢了光标位置，还把文档误置为「已修改」。
    connect(m_showSourceAction, &QAction::triggered, this, &MainWindow::toggleSourceMode);

    m_sidebarAction = new QAction(tr("侧边栏"), this);
    m_sidebarAction->setShortcut(QKeySequence(Qt::Key_F11));
    connect(m_sidebarAction, &QAction::triggered, this, &MainWindow::toggleSidebar);

    m_alwaysOnTopAction = new QAction(tr("保持窗口在最前端"), this);
    m_alwaysOnTopAction->setCheckable(true);
    connect(m_alwaysOnTopAction, &QAction::triggered, this, &MainWindow::toggleAlwaysOnTop);

    m_actualSizeAction = new QAction(tr("实际大小"), this);
    m_actualSizeAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_9));
    connect(m_actualSizeAction, &QAction::triggered, this, [this]() { updateFontSize(14); });

    m_devToolsAction = new QAction(tr("开发者工具"), this);
    m_devToolsAction->setShortcut(QKeySequence(Qt::SHIFT | Qt::Key_F12));
    connect(m_devToolsAction, &QAction::triggered, this, &MainWindow::showDevTools);
    
    // 源码模式（对应 Typora 的 Source Code Mode）
    m_toggleSourceModeAction = new QAction(QIcon(":/icons/codemode.svg"), tr("源码模式"), this);
    m_toggleSourceModeAction->setShortcut(QKeySequence(Qt::ALT | Qt::Key_S));
    m_toggleSourceModeAction->setCheckable(true);
    connect(m_toggleSourceModeAction, &QAction::triggered, this, &MainWindow::toggleSourceMode);
    
    // 智能标点（对应 Typora 的 Smart Quotes / Smart Dashes）
    m_smartPunctuationAction = new QAction(tr("智能标点"), this);
    m_smartPunctuationAction->setCheckable(true);
    m_smartPunctuationAction->setChecked(true);
    connect(m_smartPunctuationAction, &QAction::triggered, this, &MainWindow::onSmartPunctuationToggled);
    
    m_aboutAction = new QAction(tr("关于"), this);
    connect(m_aboutAction, &QAction::triggered, this, &MainWindow::showAbout);
    
    m_shortcutsAction = new QAction(tr("快捷键"), this);
    connect(m_shortcutsAction, &QAction::triggered, this, &MainWindow::showShortcuts);
}

void MainWindow::createMenus()
{
    // 菜单结构与 Typora 对齐：文件 / 编辑 / 段落 / 格式 / 视图 / 主题 / 帮助
    // 分组与顺序按 Typora 的公开界面规格对齐。

    // ---------------- 文件 ----------------
    QMenu *fileMenu = menuBar()->addMenu(tr("文件(&F)"));
    fileMenu->addAction(m_newAction);
    fileMenu->addAction(m_newWindowAction);
    fileMenu->addAction(m_openAction);
    fileMenu->addAction(m_openFolderAction);
    fileMenu->addAction(m_mergeAction);
    fileMenu->addSeparator();
    fileMenu->addAction(m_saveAction);
    fileMenu->addAction(m_saveAsAction);
    fileMenu->addAction(m_closeTabAction);
    fileMenu->addSeparator();
    // 「导出」子菜单（对齐 Typora 的 14 种格式）
    m_exportMenu = fileMenu->addMenu(tr("导出"));
    for (const Exporter::Format &f : Exporter::formats()) {
        QAction *act = m_exportMenu->addAction(f.displayName + QStringLiteral("..."));
        const QString fid = f.id;
        connect(act, &QAction::triggered, this, [this, fid]() { exportTo(fid); });
    }
    fileMenu->addSeparator();
    fileMenu->addAction(m_printAction);
    fileMenu->addSeparator();
    fileMenu->addAction(m_preferencesAction);
    fileMenu->addAction(m_preferencesFileAction);
    fileMenu->addSeparator();
    fileMenu->addAction(m_exitAction);

    // ---------------- 编辑 ----------------
    QMenu *editMenu = menuBar()->addMenu(tr("编辑(&E)"));
    editMenu->addAction(m_undoAction);
    editMenu->addAction(m_redoAction);
    editMenu->addSeparator();
    editMenu->addAction(m_cutAction);
    editMenu->addAction(m_copyAction);
    editMenu->addAction(m_pasteAction);
    editMenu->addAction(m_pasteMarkdownAction);
    editMenu->addAction(m_deleteAction);
    editMenu->addSeparator();
    editMenu->addAction(m_selectAllAction);
    editMenu->addSeparator();
    editMenu->addAction(m_copyMarkdownAction);
    editMenu->addAction(m_copyHtmlAction);
    editMenu->addAction(m_pastePlainTextAction);
    editMenu->addSeparator();

    // 智能标点（Typora 为子菜单）
    QMenu *smartMenu = editMenu->addMenu(tr("智能标点"));
    smartMenu->addAction(m_smartPunctuationAction);   // 开关：输入时转换
    smartMenu->addAction(m_smartQuotesAction);        // 智能引号
    smartMenu->addAction(m_smartDashesAction);        // 智能破折号
    smartMenu->addAction(m_unicodePunctuationAction); // Unicode 标点
    smartMenu->addSeparator();
    smartMenu->addAction(tr("更多选项..."));

    // 选择 / 删除 / 数学工具（Typora 为子菜单）
    QMenu *selectMenu = editMenu->addMenu(tr("选择"));
    selectMenu->addAction(tr("选中当前行或句"));
    selectMenu->addAction(tr("选中当前格式文本"));
    selectMenu->addAction(tr("选中当前词"));

    QMenu *deleteMenu = editMenu->addMenu(tr("删除"));
    deleteMenu->addAction(tr("删除当前行或句"));
    deleteMenu->addAction(tr("删除当前词"));
    deleteMenu->addAction(tr("删除当前格式文本"));

    editMenu->addAction(m_mathToolsAction);
    editMenu->addSeparator();
    editMenu->addAction(m_findAction);
    editMenu->addAction(m_replaceAction);
    editMenu->addSeparator();
    editMenu->addAction(m_preferencesAction);

    // ---------------- 段落 ----------------
    QMenu *paraMenu = menuBar()->addMenu(tr("段落(&P)"));
    paraMenu->addAction(m_heading1Action);
    paraMenu->addAction(m_heading2Action);
    paraMenu->addAction(m_heading3Action);
    paraMenu->addAction(m_heading4Action);
    paraMenu->addAction(m_heading5Action);
    paraMenu->addAction(m_heading6Action);
    paraMenu->addAction(m_paragraphAction);
    paraMenu->addSeparator();
    paraMenu->addAction(m_increaseHeadingAction);
    paraMenu->addAction(m_decreaseHeadingAction);
    paraMenu->addSeparator();
    paraMenu->addAction(m_tableBlockAction);
    paraMenu->addAction(m_mathBlockAction);
    paraMenu->addAction(m_codeBlockAction);
    paraMenu->addAction(m_inlineCodeAction);
    paraMenu->addAction(m_blockquoteAction);
    paraMenu->addSeparator();
    paraMenu->addAction(m_numberedListAction);
    paraMenu->addAction(m_bulletListAction);
    paraMenu->addAction(m_taskListAction);
    paraMenu->addAction(m_taskStatusAction);
    paraMenu->addAction(m_indentAction);
    paraMenu->addAction(m_outdentAction);
    paraMenu->addSeparator();
    paraMenu->addAction(m_tableAction);
    paraMenu->addAction(m_tableRowAction);
    paraMenu->addAction(m_tableColumnAction);
    paraMenu->addSeparator();
    paraMenu->addAction(m_insertParagraphAboveAction);
    paraMenu->addAction(m_insertParagraphBelowAction);
    paraMenu->addSeparator();
    paraMenu->addAction(m_inlineMathAction);
    paraMenu->addAction(m_footnoteAction);
    paraMenu->addAction(m_horizontalRuleAction);
    paraMenu->addAction(m_tocAction);
    paraMenu->addAction(m_yamlFrontMatterAction);

    // ---------------- 格式 ----------------
    QMenu *formatMenu = menuBar()->addMenu(tr("格式(&O)"));
    formatMenu->addAction(m_boldAction);
    formatMenu->addAction(m_italicAction);
    formatMenu->addAction(m_strikethroughAction);
    formatMenu->addAction(m_codeBlockAction);
    formatMenu->addAction(m_inlineCodeAction);
    formatMenu->addSeparator();
    formatMenu->addAction(m_highlightAction);
    formatMenu->addAction(m_superscriptAction);
    formatMenu->addAction(m_subscriptAction);
    formatMenu->addAction(m_commentAction);
    formatMenu->addSeparator();
    formatMenu->addAction(m_hyperlinkAction);
    formatMenu->addAction(m_imageAction);
    formatMenu->addAction(m_tableAction);
    formatMenu->addAction(m_createFootnoteAction);
    formatMenu->addSeparator();
    formatMenu->addAction(m_clearFormatAction);

    // ---------------- 视图 ----------------
    QMenu *viewMenu = menuBar()->addMenu(tr("视图(&V)"));
    viewMenu->addAction(m_toggleSidebarAction);
    viewMenu->addSeparator();
    viewMenu->addAction(m_toggleOutlineAction);
    viewMenu->addAction(m_docTreeAction);
    viewMenu->addAction(m_fileTreeAction);
    viewMenu->addSeparator();
    viewMenu->addAction(m_findAction);
    viewMenu->addSeparator();
    viewMenu->addAction(m_sourceModeAction);
    viewMenu->addAction(m_focusModeAction);
    viewMenu->addAction(m_typewriterModeAction);
    viewMenu->addAction(m_showSourceAction);
    viewMenu->addSeparator();
    viewMenu->addAction(m_sidebarAction);
    viewMenu->addAction(m_alwaysOnTopAction);
    viewMenu->addSeparator();
    viewMenu->addAction(m_actualSizeAction);
    viewMenu->addAction(m_zoomInAction);
    viewMenu->addAction(m_zoomOutAction);
    viewMenu->addSeparator();
    viewMenu->addAction(m_previewOnlyAction);
    viewMenu->addAction(m_darkModeAction);
    viewMenu->addAction(m_fullscreenAction);
    viewMenu->addSeparator();
    viewMenu->addAction(m_devToolsAction);

    // ---------------- 主题 ----------------
    QMenu *themeMenu = menuBar()->addMenu(tr("主题(&T)"));
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

    // ---------------- 帮助 ----------------
    QMenu *helpMenu = menuBar()->addMenu(tr("帮助(&H)"));
    helpMenu->addAction(m_shortcutsAction);
    helpMenu->addAction(m_aboutAction);
}

void MainWindow::createToolbars()
{
    // 文件工具栏
    QToolBar *fileToolbar = addToolBar(tr("文件"));
    fileToolbar->setIconSize(QSize(24, 24));
    fileToolbar->addAction(m_newAction);
    fileToolbar->addAction(m_openAction);
    fileToolbar->addAction(m_saveAction);
    fileToolbar->addSeparator();
    fileToolbar->addAction(m_exportHtmlAction);
    fileToolbar->addAction(m_exportPdfAction);
    
    // 编辑工具栏
    QToolBar *editToolbar = addToolBar(tr("编辑"));
    editToolbar->setIconSize(QSize(24, 24));
    editToolbar->addAction(m_undoAction);
    editToolbar->addAction(m_redoAction);
    editToolbar->addSeparator();
    editToolbar->addAction(m_cutAction);
    editToolbar->addAction(m_copyAction);
    editToolbar->addAction(m_pasteAction);
    editToolbar->addSeparator();
    editToolbar->addAction(m_findAction);
    
    // 格式工具栏
    QToolBar *formatToolbar = addToolBar(tr("格式"));
    formatToolbar->setIconSize(QSize(24, 24));
    formatToolbar->addAction(m_boldAction);
    formatToolbar->addAction(m_italicAction);
    formatToolbar->addAction(m_strikethroughAction);
    formatToolbar->addSeparator();
    formatToolbar->addAction(m_heading1Action);
    formatToolbar->addAction(m_heading2Action);
    formatToolbar->addAction(m_heading3Action);
    formatToolbar->addSeparator();
    formatToolbar->addAction(m_codeAction);
    formatToolbar->addAction(m_codeBlockAction);
    formatToolbar->addSeparator();
    formatToolbar->addAction(m_linkAction);
    formatToolbar->addAction(m_imageAction);
    formatToolbar->addAction(m_tableAction);
    formatToolbar->addSeparator();
    formatToolbar->addAction(m_bulletListAction);
    formatToolbar->addAction(m_numberedListAction);
    formatToolbar->addAction(m_blockquoteAction);
    
    // 视图工具栏
    QToolBar *viewToolbar = addToolBar(tr("视图"));
    viewToolbar->setIconSize(QSize(24, 24));
    viewToolbar->addAction(m_togglePreviewAction);
    viewToolbar->addAction(m_toggleSidebarAction);
    viewToolbar->addAction(m_toggleOutlineAction);
    viewToolbar->addAction(m_toggleSourceModeAction);
    viewToolbar->addSeparator();
    viewToolbar->addAction(m_previewOnlyAction);
    viewToolbar->addAction(m_darkModeAction);
    viewToolbar->addAction(m_focusModeAction);
    viewToolbar->addSeparator();
    viewToolbar->addAction(m_zoomInAction);
    viewToolbar->addAction(m_zoomOutAction);
}

void MainWindow::createStatusBar()
{
    statusBar()->showMessage(tr("就绪"));
    
    // 添加状态信息
    QLabel *fontSizeLabel = new QLabel(tr("字号: "), this);
    statusBar()->addPermanentWidget(fontSizeLabel);
    
    QSpinBox *fontSizeSpin = new QSpinBox(this);
    fontSizeSpin->setRange(8, 72);
    fontSizeSpin->setValue(m_fontSize);
    fontSizeSpin->setSuffix(" px");
    fontSizeSpin->setMaximumWidth(80);
    connect(fontSizeSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, &MainWindow::updateFontSize);
    statusBar()->addPermanentWidget(fontSizeSpin);
}

void MainWindow::createDockWidgets()
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
    
    // 文档树（对应 Typora 的「文档树」）：列出已打开的文档
    m_docTree = new QTreeWidget(m_sidebarTabs);
    m_docTree->setHeaderHidden(true);
    m_docTree->setIndentation(14);
    m_docTree->setAnimated(true);
    connect(m_docTree, &QTreeWidget::itemClicked, this, [this](QTreeWidgetItem *item, int) {
        const int idx = item->data(0, Qt::UserRole).toInt();
        if (idx >= 0 && idx < m_editorTabs->count()) {
            m_editorTabs->setCurrentIndex(idx);
        }
    });
    m_sidebarTabs->addTab(m_docTree, tr("文档树"));
    
    // 默认激活「文件树」页（大纲已独立成右侧 dock，见下方）
    m_sidebarTabs->setCurrentWidget(m_fileTree);
    
    fileTreeDock->setWidget(m_sidebarTabs);
    addDockWidget(Qt::LeftDockWidgetArea, fileTreeDock);
    
    // 记录 dock 以便切换显隐
    m_fileTreeDock = fileTreeDock;

    // ---- 大纲面板：独立 dock（右侧）----
    // 与左侧侧边栏（文件树 / 文档树）解耦，这样在「预览专属模式」「专注模式」里
    // 也能独立显示与开关，且只控制该 dock，中央「编辑器 | 预览」分割器完全不触碰
    // —— 开关大纲时预览渲染效果不变（只是右侧 dock 腾出 / 收回空间）。
    // 缺省开（m_toggleOutlineAction 默认勾选、dock 默认可见）。
    m_outlinePanel = new OutlinePanel(this);
    m_outlineDock = new QDockWidget(tr("大纲"), this);
    m_outlineDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    m_outlineDock->setWidget(m_outlinePanel);
    m_outlineDock->setVisible(true);
    addDockWidget(Qt::RightDockWidgetArea, m_outlineDock);
    m_outlineDock->setMinimumWidth(180);
    resizeDocks({m_outlineDock}, {240}, Qt::Horizontal);

    // 侧边栏宽度：QDockWidget 默认按内部 widget 的 sizeHint 定宽，
    // 换成 QPlainTextEdit 后中央区（splitter）的 sizeHint 变化会把它挤窄，
    // 导致大纲里的标题被省略号截断。这里给个子控件的下限 + 显式初始宽度。
    m_sidebarTabs->setMinimumWidth(200);
    resizeDocks({fileTreeDock}, {380}, Qt::Horizontal);
}

void MainWindow::createSearchReplaceDialog()
{
    m_searchDialog = new SearchReplaceDialog(this);
    m_searchDialog->setEditor(currentEditor());
}

QPlainTextEdit* MainWindow::currentEditor() const
{
    return qobject_cast<QPlainTextEdit*>(m_editorTabs->currentWidget());
}

MarkdownHighlighter* MainWindow::currentHighlighter() const
{
    QPlainTextEdit *editor = currentEditor();
    if (editor) {
        return editor->findChild<MarkdownHighlighter*>();
    }
    return nullptr;
}

void MainWindow::newFile()
{
    QPlainTextEdit *editor = new QPlainTextEdit(this);
    // Windows 上 Consolas 存在；Linux 上回退到等宽字体
    QFont monoFont = QFont("Consolas", m_fontSize);
    monoFont.setStyleHint(QFont::Monospace);
    monoFont.setFixedPitch(true);
    editor->setFont(monoFont);
    editor->setTabStopDistance(40);
    editor->setAcceptDrops(true);
    editor->setPlaceholderText(tr("开始编写Markdown..."));
    // 长行自动换行（QPlainTextEdit 默认是 WidgetWidth，但显式声明更稳，
    // 避免不同 Qt 版本默认值差异导致长段落被横向滚动条推走）
    editor->setLineWrapMode(QPlainTextEdit::WidgetWidth);
    
    // 安装事件过滤器用于拖放
    editor->installEventFilter(this);
    
    // 创建语法高亮器
    new MarkdownHighlighter(editor->document(), m_darkMode);
    
    // 应用当前主题配色
    applyEditorTheme(editor);
    
    // 连接文本变化信号
    connect(editor, &QPlainTextEdit::textChanged, this, &MainWindow::onTextChanged);
    // 光标移动 -> 大纲高亮当前章节
    connect(editor, &QPlainTextEdit::cursorPositionChanged, this, &MainWindow::onEditorCursorMoved);
    
    // 创建新标签页
    int index = m_editorTabs->addTab(editor, tr("未命名"));
    m_editorTabs->setCurrentIndex(index);
    
    // 设置初始状态为未修改
    editor->setProperty("modified", false);
    editor->setProperty("filePath", QString());
    
    updatePreview();
}

void MainWindow::setHeadlessMode(bool enabled)
{
    m_headlessMode = enabled;
}





/**
 * 自检：触发「单独预览模式」菜单项。
 *
 * 验证的是 QAction 的连接与状态切换是否正确 —— 与用户点击菜单、
 * 按快捷键走的是同一条路径（都是 QAction::triggered -> setPreviewOnlyMode）。
 */


void MainWindow::setPreviewOnlyModeAtStartup(bool enabled)
{
    if (!enabled) {
        return;
    }
    // 窗口尚未显示时布局尺寸未定，等 show() 之后再切，
    // 否则 splitter 尺寸与居中留白都会算错。
    QTimer::singleShot(0, this, [this]() {
        setPreviewOnlyMode(true);
    });
}

void MainWindow::setDarkModeAtStartup(bool enabled)
{
    // 该方法在 MainWindow 构造后、newFile() 之前调用；
    // 构造函数末尾已创建首个标签页，因此这里补刷一次所有标签页主题。
    m_darkMode = enabled;
    for (int i = 0; i < m_editorTabs->count(); ++i) {
        QPlainTextEdit *editor = qobject_cast<QPlainTextEdit*>(m_editorTabs->widget(i));
        if (!editor) continue;
        MarkdownHighlighter *hl = editor->findChild<MarkdownHighlighter*>();
        if (hl) hl->setDarkMode(enabled);
        applyEditorTheme(editor);
    }
    if (m_darkModeAction) {
        m_darkModeAction->setChecked(enabled);
    }
    updatePreview();
}

void MainWindow::openFileAtStartup(const QString &filePath)
{
    // 窗口显示后再打开文件，避免首帧闪烁
    QTimer::singleShot(0, this, [this, filePath]() {
        openFile(filePath);
        // 把文件所在目录设为当前工作目录，方便继续浏览
        QFileInfo info(filePath);
        if (info.exists()) {
            m_currentFolder = info.absolutePath();
        }
    });
}

void MainWindow::openFile()
{
    // 允许多选：一次可选择多个文件，每个文件打开为一个独立标签页。
    const QStringList files = QFileDialog::getOpenFileNames(
        this,
        tr("打开 Markdown 文件"),
        m_currentFolder,
        "Markdown Files (*.md *.markdown *.txt);;All Files (*)"
    );
    for (const QString &fileName : files) {
        if (!fileName.isEmpty()) {
            openFile(fileName);
        }
    }
}

void MainWindow::openFile(const QString &filePath)
{
    benchStart();

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QMessageBox::warning(this, tr("错误"), tr("无法打开文件: %1").arg(file.errorString()));
        return;
    }
    
    QTextStream in(&file);
    in.setEncoding(QStringConverter::Utf8);
    QString content = in.readAll();
    file.close();
    benchMark(QStringLiteral("read_file"));
    
    // 检查是否已打开
    for (int i = 0; i < m_editorTabs->count(); ++i) {
        QPlainTextEdit *editor = qobject_cast<QPlainTextEdit*>(m_editorTabs->widget(i));
        if (editor && editor->property("filePath").toString() == filePath) {
            m_editorTabs->setCurrentIndex(i);
            return;
        }
    }
    
    // 复用空标签，否则每打开一个文件都会多留一个「未命名」空标签。
    // 程序启动时会建一个初始空标签（构造函数里的 newFile），
    // 若这里不加判断，打开文件后标签栏就会是「未命名 | 文件名」两个。
    //
    // 判断条件只看「没有关联文件」+「内容为空」：
    // 不检查 modified —— 主题切换、语法高亮刷新等副作用都会触发 textChanged
    // 把 modified 置真（实测 --dark 启动时就会），拿它当条件会导致复用失效。
    // 只要没路径且没内容，复用就不会丢用户的任何东西。
    QPlainTextEdit *cur = currentEditor();
    const bool reusable = cur
            && cur->property("filePath").toString().isEmpty()
            && cur->toPlainText().trimmed().isEmpty();

    if (!reusable) {
        newFile();
    }
    QPlainTextEdit *editor = currentEditor();
    editor->setPlainText(content);
    editor->setProperty("filePath", filePath);
    // setPlainText 会触发 textChanged，把 modified 置为 true。
    // 刚载入的文件并未被用户修改，必须立刻清掉，
    // 否则标题栏与标签页会显示一个假的「已修改」星号。
    editor->setProperty("modified", false);
    m_editorTabs->setTabText(m_editorTabs->currentIndex(), QFileInfo(filePath).fileName());
    benchMark(QStringLiteral("set_editor_text"));
    
    // 更新标签页标题
    QFileInfo fileInfo(filePath);
    m_editorTabs->setTabText(m_editorTabs->currentIndex(), fileInfo.fileName());
    
    // 刷新大纲（打开文件后立即显示其标题结构）
    refreshOutline();
    benchMark(QStringLiteral("outline"));
    
    // 文件树定位到该文件所在目录
    if (m_fileModel) {
        const QString dirPath = fileInfo.absolutePath();
        if (!dirPath.isEmpty() && QDir(dirPath).exists()) {
            m_fileModel->setRootPath(dirPath);
            m_fileTree->setRootIndex(m_fileModel->index(dirPath));
        }
    }
    
    updatePreview();
    updateWindowTitle();
    if (m_benchMode) {
        // 诊断：确认「复用空标签」是否生效（应为 1）
        m_benchLog << QStringLiteral("tab_count|%1").arg(m_editorTabs->count());
    }
    benchMark(QStringLiteral("open_total"));
}

void MainWindow::openFolder()
{
    QString dir = QFileDialog::getExistingDirectory(
        this,
        tr("选择文件夹"),
        m_currentFolder,
        QFileDialog::ShowDirsOnly
    );
    
    if (!dir.isEmpty()) {
        m_currentFolder = dir;
        m_fileModel->setRootPath(dir);
        m_fileTree->setRootIndex(m_fileModel->index(dir));
    }
}

void MainWindow::mergeMarkdown()
{
    MergeDialog dlg(this);
    if (dlg.exec() != QDialog::Accepted) {
        return;
    }

    const QString text = dlg.mergedText();
    openMergedContent(text);

    // 「合并并保存」：开新标签后立刻弹出保存对话框
    if (dlg.saveRequested()) {
        saveFileAs();
    }
}

void MainWindow::openMergedContent(const QString &text)
{
    // 复用启动时的空白「未命名」标签（与 openFile 的复用逻辑一致），
    // 避免每合并一次就多留一个空标签。
    QPlainTextEdit *cur = currentEditor();
    const bool reusable = cur
            && cur->property("filePath").toString().isEmpty()
            && cur->toPlainText().trimmed().isEmpty();
    if (!reusable) {
        newFile();
    }
    QPlainTextEdit *editor = currentEditor();
    editor->setPlainText(text);
    editor->setProperty("filePath", QString());
    // 合并结果是新内容、尚未落盘，标记为已修改以提示保存
    editor->setProperty("modified", true);
    m_editorTabs->setTabText(m_editorTabs->currentIndex(), tr("合并结果"));

    refreshOutline();
    updatePreview();
    updateWindowTitle();
}

bool MainWindow::saveCurrentTab()
{
    QPlainTextEdit *editor = currentEditor();
    if (!editor) return false;
    
    QString filePath = editor->property("filePath").toString();
    
    if (filePath.isEmpty()) {
        if (saveFileAs()) {
            return true;
        }
        return false;
    }
    
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, tr("错误"), tr("无法保存文件: %1").arg(file.errorString()));
        return false;
    }
    
    QTextStream out(&file);
    out.setEncoding(QStringConverter::Utf8);
    out << editor->toPlainText();
    out.flush();
    file.close();
    
    // 清除修改标志
    m_editorTabs->currentWidget()->setProperty("modified", false);
    
    statusBar()->showMessage(tr("文件已保存"), 2000);
    return true;
}

void MainWindow::saveFile()
{
    saveCurrentTab();
}

bool MainWindow::saveFileAs()
{
    QString fileName = QFileDialog::getSaveFileName(
        this,
        tr("保存 Markdown 文件"),
        m_currentFolder,
        "Markdown Files (*.md);;All Files (*)"
    );
    
    if (!fileName.isEmpty()) {
        QPlainTextEdit *editor = currentEditor();
        editor->setProperty("filePath", fileName);
        
        QFileInfo fileInfo(fileName);
        m_editorTabs->setTabText(m_editorTabs->currentIndex(), fileInfo.fileName());
        
        return saveCurrentTab();
    }
    return false;
}

void MainWindow::closeTab(int index)
{
    QWidget *widget = m_editorTabs->widget(index);
    if (!widget) return;
    
    // 检查是否有未保存的更改
    if (widget->property("modified").toBool()) {
        QMessageBox::StandardButton reply = QMessageBox::question(
            this,
            tr("保存更改"),
            tr("文件尚未保存，是否保存？"),
            QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel
        );
        
        if (reply == QMessageBox::Save) {
            m_editorTabs->setCurrentIndex(index);
            if (!saveCurrentTab()) {
                return;
            }
        } else if (reply == QMessageBox::Cancel) {
            return;
        }
    }
    
    m_editorTabs->removeTab(index);
    widget->deleteLater();
    
    // 如果没有标签页了，创建一个新的
    if (m_editorTabs->count() == 0) {
        newFile();
    }
}





void MainWindow::undo()
{
    if (currentEditor()) currentEditor()->undo();
}

void MainWindow::redo()
{
    if (currentEditor()) currentEditor()->redo();
}

void MainWindow::cut()
{
    if (currentEditor()) currentEditor()->cut();
}

void MainWindow::copy()
{
    if (currentEditor()) currentEditor()->copy();
}

void MainWindow::paste()
{
    if (currentEditor()) currentEditor()->paste();
}

void MainWindow::selectAll()
{
    if (currentEditor()) currentEditor()->selectAll();
}

void MainWindow::find()
{
    if (m_searchDialog) {
        m_searchDialog->setEditor(currentEditor());
        m_searchDialog->show();
        m_searchDialog->activateWindow();
    }
}

void MainWindow::replace()
{
    if (m_searchDialog) {
        m_searchDialog->setEditor(currentEditor());
        m_searchDialog->showReplace();
        m_searchDialog->show();
        m_searchDialog->activateWindow();
    }
}





































void MainWindow::togglePreview()
{
    m_previewVisible = !m_previewVisible;
    m_preview->setVisible(m_previewVisible);
    m_togglePreviewAction->setChecked(m_previewVisible);
}

void MainWindow::toggleSidebar()
{
    m_sidebarVisible = !m_sidebarVisible;
    
    // m_fileTree 的 parent 是 m_sidebarTabs（tab 结构），不是 QDockWidget，
    // 必须用 m_fileTreeDock 才能控制侧边栏显隐。
    if (m_fileTreeDock) {
        m_fileTreeDock->setVisible(m_sidebarVisible);
    }
    
    m_toggleSidebarAction->setChecked(m_sidebarVisible);
}

void MainWindow::toggleFullscreen()
{
    if (isFullScreen()) {
        showNormal();
    } else {
        showFullScreen();
    }
}

void MainWindow::setDarkMode(bool enabled)
{
    m_darkMode = enabled;
    m_darkModeAction->setChecked(enabled);
    
    // 更新所有标签页的高亮器与配色
    for (int i = 0; i < m_editorTabs->count(); ++i) {
        QPlainTextEdit *editor = qobject_cast<QPlainTextEdit*>(m_editorTabs->widget(i));
        if (editor) {
            MarkdownHighlighter *highlighter = editor->findChild<MarkdownHighlighter*>();
            if (highlighter) {
                highlighter->setDarkMode(enabled);
            }
            applyEditorTheme(editor);
        }
    }
    
    // 更新预览
    updatePreview();
}

void MainWindow::applyEditorTheme(QPlainTextEdit *editor)
{
    if (!editor) return;
    
    if (m_darkMode) {
        editor->setStyleSheet(
            "QPlainTextEdit { background-color: #1e1e1e; color: #d4d4d4; "
            "border: 1px solid #333333; selection-background-color: #264f78; }"
        );
    } else {
        editor->setStyleSheet(
            "QPlainTextEdit { background-color: #ffffff; color: #333333; "
            "border: 1px solid #dddddd; selection-background-color: #b4d5fe; }"
        );
    }
}

/**
 * 单独预览模式。
 *
 * 只保留预览区，隐藏编辑器与侧边栏，用于专注阅读 / 演示。
 * 与专注模式（Focus Mode）互斥：
 *   - 专注模式隐藏工具栏、菜单栏、状态栏和侧边栏，保留编辑器
 *   - 单独预览模式保留菜单栏和状态栏（否则无法退出），隐藏编辑器和侧边栏
 */
void MainWindow::setPreviewOnlyMode(bool enabled)
{
    if (m_previewOnlyMode == enabled) {
        return;
    }
    m_previewOnlyMode = enabled;
    m_previewOnlyAction->setChecked(enabled);

    if (enabled) {
        // 与专注模式互斥：进入前先退出专注模式
        if (m_focusMode) {
            setFocusMode(false);
        }
        // 与源码模式互斥：源码模式下预览不可见，先退出。
        // 走 setSourceMode(false) 而不是直接改 m_sourceMode，
        // 这样菜单勾选状态与语法高亮才会一并恢复。
        setSourceMode(false);

        // 单独预览模式的前提就是预览可见，这里显式打开 ——
        // 从源码模式切过来时，预览刚被源码模式关掉。
        m_preview->setVisible(true);

        m_editorTabs->setVisible(false);
        if (m_fileTreeDock) {
            m_fileTreeDock->setVisible(false);
        }
        // 预览专属模式下仍显示标签栏，便于切换其它已打开的文件。
        if (m_previewTabs) {
            rebuildPreviewTabs();
            m_previewTabs->setVisible(m_editorTabs->count() > 0);
        }
        // 预览区独占中央区域。
        // 隐藏编辑器后 splitter 会自动把空间让给预览区，
        // 这里再把 stretch 权重全给预览区，确保它吃满可用宽度。
        m_mainSplitter->setStretchFactor(0, 0);
        m_mainSplitter->setStretchFactor(1, 1);
        m_mainSplitter->setSizes(QList<int>() << 0 << 1);
        m_preview->setFocus();
        // 布局完成后预览区宽度才最终确定，此时重算居中留白
        QTimer::singleShot(0, this, [this]() { applyPreviewCentering(); });
        QTimer::singleShot(150, this, [this]() { applyPreviewCentering(); });
    } else {
        m_editorTabs->setVisible(true);
        if (m_previewTabs) {
            m_previewTabs->setVisible(false);
        }
        if (m_fileTreeDock) {
            m_fileTreeDock->setVisible(m_sidebarVisible);
        }
        // 恢复编辑/预览各占一半
        m_mainSplitter->setStretchFactor(0, 1);
        m_mainSplitter->setStretchFactor(1, 1);
        m_mainSplitter->setSizes(QList<int>() << 5000 << 5000);
        // 布局完成后重算居中留白
        QTimer::singleShot(0, this, [this]() { applyPreviewCentering(); });
        QTimer::singleShot(150, this, [this]() { applyPreviewCentering(); });
    }

    statusBar()->showMessage(enabled ? tr("已切换到单独预览模式（Ctrl+Shift+P 返回编辑）")
                                     : tr("已返回编辑模式"), 2500);
}

void MainWindow::setFocusMode(bool enabled)
{
    // 专注模式与单独预览模式互斥
    if (enabled && m_previewOnlyMode) {
        setPreviewOnlyMode(false);
    }

    m_focusMode = enabled;
    m_focusModeAction->setChecked(enabled);
    
    if (enabled) {
        // 隐藏所有工具栏和状态栏
        for (QToolBar *toolbar : findChildren<QToolBar*>()) {
            toolbar->setVisible(false);
        }
        menuBar()->setVisible(false);
        statusBar()->setVisible(false);
        
        // 隐藏侧边栏
        // 注意：m_fileTree 的 parent 现在是 m_sidebarTabs（tab 结构），
        // 不再是 QDockWidget，必须用 m_fileTreeDock 才能取到真正的 dock。
        if (m_fileTreeDock) {
            m_fileTreeDock->setVisible(false);
        }
        // 隐藏独立大纲 dock（专注模式下一并收起）
        if (m_outlineDock) {
            m_outlineDock->setVisible(false);
        }
        
        // 隐藏预览
        m_preview->setVisible(false);
    } else {
        // 恢复所有界面元素
        for (QToolBar *toolbar : findChildren<QToolBar*>()) {
            toolbar->setVisible(true);
        }
        menuBar()->setVisible(true);
        statusBar()->setVisible(true);
        
        // 恢复侧边栏
        if (m_fileTreeDock) {
            m_fileTreeDock->setVisible(m_sidebarVisible);
        }
        // 恢复大纲 dock（按大纲开关的勾选态）
        if (m_outlineDock) {
            m_outlineDock->setVisible(m_toggleOutlineAction->isChecked());
        }
        
        // 恢复预览
        m_preview->setVisible(m_previewVisible);
    }
}

void MainWindow::updateFontSize(int size)
{
    m_fontSize = size;
    
    for (int i = 0; i < m_editorTabs->count(); ++i) {
        QPlainTextEdit *editor = qobject_cast<QPlainTextEdit*>(m_editorTabs->widget(i));
        if (editor) {
            QFont font = editor->font();
            font.setPointSize(size);
            editor->setFont(font);
        }
    }
    
    updatePreview();
}

void MainWindow::onTabChanged(int index)
{
    Q_UNUSED(index);
    QPlainTextEdit *editor = currentEditor();
    if (editor) {
        QFont font = editor->font();
        font.setPointSize(m_fontSize);
        editor->setFont(font);
    }
    
    // 大纲随当前标签页切换（Typora 的 Outline 行为）
    refreshOutline();
    // 文档树同步
    refreshDocTree();
    
    updatePreview();
    updateStatusBar();
    // 标题栏跟随当前文档（"文件名 - Markdown Editor"）
    updateWindowTitle();

    // 单独预览模式下同步预览标签栏（切换文件时高亮跟随）
    if (m_previewOnlyMode && m_previewTabs) {
        if (m_previewTabs->count() != m_editorTabs->count()) {
            rebuildPreviewTabs();
        } else {
            m_syncingPreviewTabs = true;
            for (int i = 0; i < m_editorTabs->count(); ++i) {
                m_previewTabs->setTabText(i, m_editorTabs->tabText(i));
            }
            m_previewTabs->setCurrentIndex(m_editorTabs->currentIndex());
            m_syncingPreviewTabs = false;
        }
    }
}

void MainWindow::onPreviewTabChanged(int index)
{
    if (m_syncingPreviewTabs || index < 0) {
        return;
    }
    if (index < m_editorTabs->count()) {
        m_editorTabs->setCurrentIndex(index);
    }
}

void MainWindow::rebuildPreviewTabs()
{
    if (!m_previewTabs) {
        return;
    }
    m_syncingPreviewTabs = true;
    while (m_previewTabs->count() > 0) {
        m_previewTabs->removeTab(0);
    }
    for (int i = 0; i < m_editorTabs->count(); ++i) {
        m_previewTabs->addTab(m_editorTabs->tabText(i));
    }
    if (m_editorTabs->count() > 0) {
        m_previewTabs->setCurrentIndex(m_editorTabs->currentIndex());
    }
    m_syncingPreviewTabs = false;
}

void MainWindow::onTextChanged()
{
    // 内容版本号自增。
    // 预览、大纲、全文三份缓存都以它为键，避免每次都 toPlainText()
    // 拷贝整份文档来比对内容是否变化。
    ++m_contentVersion;

    // 无界面模式下不触发任何交互式副作用
    if (m_headlessMode) {
        return;
    }

    // 设置修改标志
    QObject *senderObj = sender();
    QWidget *widget = qobject_cast<QWidget*>(senderObj);
    if (widget) {
        widget->setProperty("modified", true);
    }
    
    // 重置预览更新计时器
    if (m_previewUpdateTimer) {
        m_previewUpdateTimer->start(500);
    }
    
    // 刷新大纲（Typora 的 Outline 随内容实时更新）
    // 用独立的短延时，避免每次按键都重扫全文
    if (m_outlineTimer) {
        m_outlineTimer->start(300);
    }
    
    // 更新标签页标题显示修改标志
    int index = m_editorTabs->indexOf(qobject_cast<QWidget*>(sender()));
    if (index >= 0) {
        QString title = m_editorTabs->tabText(index);
        if (!title.endsWith("*") && widget->property("modified").toBool()) {
            m_editorTabs->setTabText(index, title + "*");
        }
    }
}

void MainWindow::updatePreview()
{
    QPlainTextEdit *editor = currentEditor();
    if (!editor) return;

    // 内容未变则跳过重渲染。
    // 打开文件时 updatePreview 会被触发多次（newFile 一次、openFile 一次、
    // 防抖定时器再补一次），大文档下每次都是几百毫秒的开销，必须去重。
    //
    // 判据是「编辑器 + 内容版本号」，不再拷贝全文来比对：
    // 拷贝全文在 5MB 文档上就是 10MB 分配，比重渲染本身还贵。
    if (editor == m_previewCacheEditor && m_contentVersion == m_previewCacheVersion) {
        benchMark(QStringLiteral("preview_skipped"));
        return;
    }
    m_previewCacheEditor = editor;
    m_previewCacheVersion = m_contentVersion;

    // 只渲染 body 片段，不带 <style>/<script>。
    //
    // QTextBrowser 不认 <style> 与 <script>（那几 KB 每帧都要解析一遍，
    // 纯浪费）；预览区的样式来自 QTextDocument::setDefaultStyleSheet，
    // 与 HTML 内容无关。整页 HTML 只在「导出为 HTML」时才需要。
    const QString html = generateHtmlBody(currentTextCached());
    benchMark(QStringLiteral("preview_generateHtml"));

    m_preview->setHtml(html);
    benchMark(QStringLiteral("preview_setHtml"));

    // 居中留白：
    // QTextBrowser 的 HTML 子集既不支持 margin:auto，也忽略 body/div 的 margin 与 padding，
    // 所以改用 Qt 原生 API：QTextDocument 根 frame 的左右边距。
    // 这是在 setHtml 之后设置的（setHtml 会重置文档格式）。
    applyPreviewCentering();
}

/**
 * 取当前编辑器的全文（带缓存）。
 *
 * 预览区与大纲面板都要用全文。早期两边各调一次 toPlainText()，
 * 就是两份整篇文档的拷贝；这里让它们共用一份。
 */
const QString &MainWindow::currentTextCached()
{
    QPlainTextEdit *editor = currentEditor();
    if (!editor) {
        m_textCache.clear();
        return m_textCache;
    }
    if (editor != m_textCacheEditor || m_contentVersion != m_textCacheVersion) {
        m_textCacheEditor = editor;
        m_textCacheVersion = m_contentVersion;
        m_textCache = editor->toPlainText();
    }
    return m_textCache;
}

/**
 * 给预览区应用左右对称的居中边距。
 */
void MainWindow::applyPreviewCentering()
{
    if (!m_preview) {
        return;
    }
    QTextDocument *doc = m_preview->document();
    if (!doc || !doc->rootFrame()) {
        return;
    }

    const int avail = m_preview->viewport()->width();
    // 居中的目标宽度：取主题设定宽度与 860px 中的较大值。
    // 860 是长文阅读的舒适宽度；窄栏主题（如报刊风 700px）居中后两侧留白会过多。
    const int maxW = qMax(Theme::palette(m_themeId).contentWidth, 860);

    int pad = 0;
    if (avail > maxW + 80) {
        pad = (avail - maxW) / 2;
    }
    // 保留一点基础留白
    const int base = 20;
    pad += base;

    QTextFrameFormat fmt = doc->rootFrame()->frameFormat();
    fmt.setLeftMargin(pad);
    fmt.setRightMargin(pad);
    doc->rootFrame()->setFrameFormat(fmt);
}

/**
 * 预览区居中留白。
 *
 * QTextBrowser 的 HTML 子集不支持 margin:auto，无法用 CSS 让正文居中。
 * 这里按预览区实际宽度计算左右对称的 padding 并重新生成 HTML。
 * 为避免频繁重排，仅在宽度变化超过 40px 时才刷新。
 */
void MainWindow::updatePreviewCentering()
{
    if (m_headlessMode || !m_preview) {
        return;
    }
    // 只调整根 frame 的边距，不重新生成 HTML，开销很小，
    // 所以不需要阈值去抖 —— 布局过程中的每一帧都重新计算，
    // 这样窗口尺寸稳定时得到的边距一定是准的。
    applyPreviewCentering();
}

void MainWindow::resizeEvent(QResizeEvent *event)
{
    QMainWindow::resizeEvent(event);
    // 窗口尺寸变化会影响预览区的居中留白
    updatePreviewCentering();
    // 单独预览模式下，编辑器已隐藏，需要让 splitter 重新分配空间，
    // 否则预览区会停留在旧的宽度上（窗口缩小后右侧留白异常）。
    if (m_previewOnlyMode) {
        m_mainSplitter->setSizes(QList<int>() << 0 << 1);
    }
}

QString MainWindow::generateHtml(const QString &markdown)
{
    // 预览与 HTML 导出都走同一条路径：
    //   DocModel::parse()            ->  IR（块级 + 行内）
    //   HtmlRenderer::renderBody()   ->  HTML
    //
    // 早期这里是一套独立的正则状态机，与 DocModel 对同一份文档给出不同结果：
    // 表格多出首尾两个空列、行内代码里的 ** 变成真粗体、~~~ 代码块整个失效、
    // 嵌套列表被拍平、*** 粗斜体多出一个孤立 *……
    // 结果就是「预览里看到的」和「导出出来的」对不上。
    // 一份文档只允许有一套解析规则，现在只有 DocModel 这一套。
    // 构建完整的 HTML 页面
    const QString css = Theme::previewCss(m_themeId);

    return QString(
        "<!DOCTYPE html>\n"
        "<html>\n"
        "<head>\n"
        "<meta charset=\"UTF-8\">\n"
        "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">\n"
        "<style>%1</style>\n"
        "<script>%2</script>\n"
        "</head>\n"
        "<body>\n"
        "%3\n"
        "</body>\n"
        "</html>"
    ).arg(css).arg(getDefaultJs()).arg(generateHtmlBody(markdown));
}

/**
 * 渲染 HTML body 片段（带 .markdown-body 容器）。
 *
 * 预览区用它（QTextBrowser 不认 <style>/<script>，整页 HTML 纯属浪费）；
 * 整页导出在它外面再包一层 <html>/<head>。
 */
QString MainWindow::generateHtmlBody(const QString &markdown)
{
    const QList<DocBlock> blocks = DocModel::parse(markdown);
    return QStringLiteral("<div class=\"markdown-body\">\n%1\n</div>")
        .arg(HtmlRenderer::renderBody(blocks));
}


QString MainWindow::getDefaultCss()
{
    // 预览区 CSS 统一由主题系统生成。
    //
    // 主题与「深色模式」是相互独立的两个维度（与 Typora 一致）：
    //   - 主题决定预览区的整体配色（用户显式选择，不被深色模式覆盖）
    //   - 深色模式只影响编辑器的语法高亮配色与背景
    // 这样用户在 github 主题下开启深色模式，预览区仍保持 github 风格。
    return Theme::previewCss(m_themeId);
}

QString MainWindow::getDefaultJs()
{
    return R"(
        // 代码高亮初始化
        document.addEventListener('DOMContentLoaded', function() {
            // 可以添加额外的JavaScript功能
        });
    )";
}

void MainWindow::onFileTreeClicked(const QModelIndex &index)
{
    Q_UNUSED(index);
    // 单击选中
}

void MainWindow::onFileTreeDoubleClicked(const QModelIndex &index)
{
    QString filePath = m_fileModel->filePath(index);
    QFileInfo fileInfo(filePath);
    
    if (fileInfo.isFile() && (fileInfo.suffix() == "md" || fileInfo.suffix() == "markdown" || fileInfo.suffix() == "txt")) {
        openFile(filePath);
    }
}

void MainWindow::updateStatusBar()
{
    QPlainTextEdit *editor = currentEditor();
    if (!editor) return;

    // 字符数与行数直接问 QTextDocument，都是 O(1)。
    //
    // 早期实现是 toPlainText() 拿全文拷贝，再 split(QRegularExpression("\\s+"))
    // 做全文正则扫描、再 split("\n") 数行 —— 这个定时器每秒触发一次，
    // 5MB 文档就是每秒几十万个 QString 对象的分配，且是程序开着就一直在付。
    //
    // 单词数没有 O(1) 的等价物（要按语言分词），改为按需触发：
    // 只在内容真正变化时重算，静止时沿用上次的结果。
    // 用 document()->revision() 判断变化 —— 它在每次内容修改时自增；
    // 只比字符数是不够的（把 "a" 改成 "b" 字符数不变）。
    const QTextDocument *doc = editor->document();
    const int chars = doc->characterCount();
    const int lines = doc->blockCount();
    const int revision = doc->revision();

    if (editor != m_lastStatusEditor || revision != m_lastStatusRevision) {
        m_lastStatusEditor = editor;
        m_lastStatusRevision = revision;
        m_lastStatusWords = countWords(editor->toPlainText());
    }

    statusBar()->showMessage(
        tr("字符: %1 | 单词: %2 | 行: %3").arg(chars).arg(m_lastStatusWords).arg(lines)
    );
}

/**
 * 统计单词数。
 *
 * 按空白切分并忽略空段。中文没有空格分隔，英文按词、中文按字近似计数，
 * 与早期实现的行为保持一致。
 */
int MainWindow::countWords(const QString &text)
{
    if (text.isEmpty()) {
        return 0;
    }
    return text.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts).count();
}

void MainWindow::onAutoSaveTimer()
{
    // 自动保存所有已修改的文件
    for (int i = 0; i < m_editorTabs->count(); ++i) {
        QWidget *widget = m_editorTabs->widget(i);
        if (widget->property("modified").toBool()) {
            QString filePath = widget->property("filePath").toString();
            if (!filePath.isEmpty()) {
                m_editorTabs->setCurrentIndex(i);
                saveCurrentTab();
            }
        }
    }
}





void MainWindow::loadSettings()
{
    QSettings settings;
    restoreGeometry(settings.value("geometry").toByteArray());
    restoreState(settings.value("windowState").toByteArray());
    
    m_fontSize = settings.value("fontSize", 14).toInt();
    m_darkMode = settings.value("darkMode", false).toBool();
    m_currentFolder = settings.value("currentFolder", QDir::homePath()).toString();
    // 主题（Typora 风格功能）
    m_themeId = settings.value("theme", Theme::ThemeId::Default).toString();
    if (!Theme::availableThemes().contains(m_themeId)) {
        m_themeId = Theme::ThemeId::Default;
    }
    m_smartPunctuation->setEnabled(settings.value("smartPunctuation", true).toBool());
    
    // 文件树根路径：原版从未在启动时设置 setRootPath，
    // 导致文件树永远是空的。此处用当前工作目录初始化。
    if (m_fileModel && !m_currentFolder.isEmpty()) {
        QDir dir(m_currentFolder);
        if (dir.exists()) {
            m_fileModel->setRootPath(m_currentFolder);
            m_fileTree->setRootIndex(m_fileModel->index(m_currentFolder));
        }
    }
}

void MainWindow::saveSettings()
{
    QSettings settings;
    settings.setValue("geometry", saveGeometry());
    settings.setValue("windowState", saveState());
    settings.setValue("fontSize", m_fontSize);
    settings.setValue("darkMode", m_darkMode);
    settings.setValue("currentFolder", m_currentFolder);
    // 主题（Typora 风格功能）
    settings.setValue("theme", m_themeId);
    settings.setValue("smartPunctuation", m_smartPunctuation->isEnabled());
}

bool MainWindow::confirmCloseTab(int index)
{
    QWidget *widget = m_editorTabs->widget(index);
    if (!widget) return true;
    
    if (widget->property("modified").toBool()) {
        QMessageBox::StandardButton reply = QMessageBox::question(
            this,
            tr("保存更改"),
            tr("文件尚未保存，是否保存？"),
            QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel
        );
        
        if (reply == QMessageBox::Save) {
            m_editorTabs->setCurrentIndex(index);
            return saveCurrentTab();
        } else if (reply == QMessageBox::Cancel) {
            return false;
        }
    }
    
    return true;
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    // 检查所有标签页 - 从后往前遍历
    for (int i = m_editorTabs->count() - 1; i >= 0; --i) {
        QWidget *widget = m_editorTabs->widget(i);
        if (!widget) continue;
        
        // 检查文件是否已修改
        bool isModified = widget->property("modified").toBool();
        
        if (isModified) {
            // 文件已修改，弹出保存对话框
            m_editorTabs->setCurrentIndex(i);
            
            QString filePath = widget->property("filePath").toString();
            QString message;
            if (filePath.isEmpty()) {
                message = tr("文件尚未保存，是否保存？");
            } else {
                message = tr("文件 \"") + filePath + tr("\" 已修改，是否保存？");
            }
            
            QMessageBox::StandardButton reply = QMessageBox::question(
                this,
                tr("保存更改"),
                message,
                QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel
            );
            
            if (reply == QMessageBox::Save) {
                if (filePath.isEmpty()) {
                    if (!saveFileAs()) {
                        event->ignore();
                        return;
                    }
                } else {
                    if (!saveCurrentTab()) {
                        event->ignore();
                        return;
                    }
                }
            } else if (reply == QMessageBox::Cancel) {
                // 用户取消，停止关闭
                event->ignore();
                return;
            }
            // 如果选择 Discard，继续关闭
        }
    }
    
    event->accept();
}

void MainWindow::dragEnterEvent(QDragEnterEvent *event)
{
    // 允许从资源管理器拖入 .md 文件
    if (event->mimeData()->hasUrls()) {
        event->acceptProposedAction();
    } else {
        QMainWindow::dragEnterEvent(event);
    }
}

void MainWindow::dropEvent(QDropEvent *event)
{
    const QList<QUrl> urls = event->mimeData()->urls();
    int opened = 0;
    // 支持一次拖入多个文件：每个文件打开为一个独立标签页。
    for (const QUrl &url : urls) {
        if (url.isLocalFile()) {
            QString path = url.toLocalFile();
            QFileInfo info(path);
            if (info.isFile() &&
                (info.suffix() == "md" || info.suffix() == "markdown" || info.suffix() == "txt")) {
                openFile(path);
                ++opened;
            }
        }
    }
    if (opened > 0) {
        event->acceptProposedAction();
        return;
    }
    QMainWindow::dropEvent(event);
}

/**
 * 事件过滤器：处理编辑器的智能标点输入。
 *
 * 对应 Typora 的「编辑 > 智能标点」（Smart Quotes / Smart Dashes）。
 * 实现方式：拦截 KeyPress 事件，对引号与破折号做转换后手动插入。
 */
bool MainWindow::eventFilter(QObject *watched, QEvent *event)
{
    // 预览区尺寸变化 -> 重算居中留白
    // （拖动分割条不会触发 MainWindow 的 resizeEvent）
    if (event->type() == QEvent::Resize && watched == m_preview) {
        applyPreviewCentering();
        return QMainWindow::eventFilter(watched, event);
    }

    if (event->type() == QEvent::KeyPress && m_smartPunctuation) {
        QKeyEvent *keyEvent = static_cast<QKeyEvent*>(event);
        QPlainTextEdit *editor = qobject_cast<QPlainTextEdit*>(watched);

        if (editor && !editor->isReadOnly() && keyEvent->text().size() == 1) {
            // 不处理带 Ctrl/Alt/Shift 组合的按键（那是快捷键）
            if (keyEvent->modifiers() == Qt::NoModifier) {
                QTextCursor cursor = editor->textCursor();
                const QChar prev = cursor.position() > 0
                        ? editor->document()->characterAt(cursor.position() - 1)
                        : QChar();
                const QChar next = cursor.position() < editor->document()->characterCount() - 1
                        ? editor->document()->characterAt(cursor.position() + 1)
                        : QChar();

                QString replacement;
                int caretBack = 0;
                if (m_smartPunctuation->process(keyEvent->text().at(0).toLatin1(),
                                               prev, next, replacement, caretBack)) {
                    if (caretBack > 0 && cursor.position() > 0) {
                        // 破折号/省略号：吃掉前一个已输入的重复字符
                        cursor.deletePreviousChar();
                    }
                    cursor.insertText(replacement);
                    editor->setTextCursor(cursor);
                    return true;   // 拦截，不再交给 QPlainTextEdit 默认处理
                }
            }
        }
    }

    return QMainWindow::eventFilter(watched, event);
}

void MainWindow::keyPressEvent(QKeyEvent *event)
{
    // Ctrl+N / Ctrl+O / Ctrl+S / Ctrl+W 都已经是 QAction 快捷键，
    // 由 QShortcutMap 统一分发，与焦点在哪个控件无关。
    // 这里不再重复处理（原来的裸 switch 在编辑器有焦点时根本收不到事件）。
    QMainWindow::keyPressEvent(event);
}


// ===========================================================================
// Typora 风格功能实现
// ===========================================================================

/**
 * 大纲面板显隐切换（对应 Typora 的「视图 > 大纲」）。
 */
/**
 * 显示/隐藏大纲。
 *
 * 侧边栏现在有两个页签（文件树 / 大纲），这里的语义是：
 * 若当前不在大纲页则切到大纲页；若已在大纲页则折叠/展开侧边栏。
 * 单独预览模式下只切页签，不动侧边栏显隐（预览模式已整体隐藏侧边栏）。
 */
void MainWindow::toggleOutline(bool checked)
{
    if (!m_outlineDock) {
        return;
    }
    // 大纲现在是独立的右侧 dock：勾选=显示，取消=隐藏。
    // 只动该 dock，中央「编辑器 | 预览」分割器完全不触碰，预览渲染不受影响。
    m_outlineDock->setVisible(checked);
    m_toggleOutlineAction->setChecked(checked);
}

/**
 * 源码模式切换（对应 Typora 的 Source Code Mode）。
 *
 * Typora 的源码模式隐藏所有富文本渲染与预览，只显示原始 Markdown 源码。
 * 本实现：隐藏预览区、把编辑器切为纯文本显示、关闭语法高亮。
 */
void MainWindow::toggleSourceMode()
{
    setSourceMode(!m_sourceMode);
    statusBar()->showMessage(m_sourceMode ? tr("源码模式已启用") : tr("源码模式已关闭"), 2000);
}

/**
 * 设置源码模式。
 *
 * 所有会改变源码模式的地方都必须走这里。除了切 m_sourceMode，
 * 还要同步两个勾选项、切换预览区可见性、开关语法高亮。
 *
 * 之前这些逻辑散落在两处，且都不完整：
 *   - toggleSourceMode()：只同步了工具栏的勾选项，视图菜单的
 *     「源代码模式」(Ctrl+/) 永远显示未勾选；停用高亮写的是
 *     setDocument(document())，传入同一个指针，Qt 首行就 return，等于没做。
 *   - setPreviewOnlyMode()：手动把 m_sourceMode 置 false 时，
 *     漏了菜单勾选，也漏了把高亮重新打开 —— 结果从单独预览模式退出来之后，
 *     编辑器的高亮一直是关着的。
 */
void MainWindow::setSourceMode(bool enabled)
{
    if (m_sourceMode == enabled) {
        return;
    }
    m_sourceMode = enabled;

    // 同步所有表示「源码模式」的勾选项
    m_toggleSourceModeAction->setChecked(m_sourceMode);
    m_sourceModeAction->setChecked(m_sourceMode);

    m_preview->setVisible(!m_sourceMode && m_previewVisible);

    // 源码模式下停用语法高亮（高亮会干扰阅读原始标记）
    for (int i = 0; i < m_editorTabs->count(); ++i) {
        QPlainTextEdit *editor = qobject_cast<QPlainTextEdit*>(m_editorTabs->widget(i));
        if (!editor) continue;
        MarkdownHighlighter *hl = editor->findChild<MarkdownHighlighter*>();
        if (hl) {
            hl->setHighlightingEnabled(!m_sourceMode);
        }
    }
}

/**
 * 应用主题（对应 Typora 的「视图 > 主题」）。
 */
void MainWindow::applyThemeById(const QString &themeId)
{
    m_themeId = themeId;
    applyCurrentTheme();
    saveSettings();
}

/**
 * 临时应用主题（不写入设置）。
 *
 * 命令行 --theme 用这个：一次性指定配色，不该覆盖用户保存的偏好。
 */
void MainWindow::applyThemeTemporarily(const QString &themeId)
{
    m_themeId = themeId;
    applyCurrentTheme();
}

/**
 * 把当前主题应用到编辑器、预览区、大纲面板。
 */
void MainWindow::applyCurrentTheme()
{
    // 预览区 CSS
    const QString css = Theme::previewCss(m_themeId);
    m_preview->document()->setDefaultStyleSheet(css);

    // 换主题不需要重渲染预览：预览区的样式来自上面的 setDefaultStyleSheet，
    // 而预览区的 HTML 内容（generateHtmlBody）与主题无关。
    // 早期预览 HTML 里内嵌了整份 CSS，切主题要连 HTML 一起重建 ——
    // 那份 CSS QTextBrowser 根本不认，纯属白费力气。

    // 同步主题菜单的选中态
    if (QMenu *themeMenu = findChild<QMenu*>("themeMenu")) {
        const QList<QAction*> acts = themeMenu->actions();
        for (QAction *act : acts) {
            if (act->data().toString() == m_themeId) {
                act->setChecked(true);
                break;
            }
        }
    }

    // 所有编辑器
    for (int i = 0; i < m_editorTabs->count(); ++i) {
        QPlainTextEdit *editor = qobject_cast<QPlainTextEdit*>(m_editorTabs->widget(i));
        if (editor) {
            editor->setStyleSheet(Theme::editorStyleSheet(m_themeId));
        }
    }

    // 大纲面板
    if (m_outlinePanel) {
        m_outlinePanel->applyTheme(m_themeId);
    }

    // 注意：主题与深色模式相互独立，不做联动 ——
    // 用户显式选择了 github 主题，就不该被「深色模式」开关改成 github-dark。
    // 深色主题（如 night / github-dark）本身就自带深色配色，无需额外切换。

    updatePreview();
}

/**
 * 大纲点击 -> 跳转到编辑器对应行。
 */
void MainWindow::onOutlineNavigate(int line)
{
    QPlainTextEdit *editor = currentEditor();
    if (!editor) {
        return;
    }

    // 定位到该行
    QTextCursor cursor = editor->textCursor();
    cursor.movePosition(QTextCursor::Start);
    cursor.movePosition(QTextCursor::Down, QTextCursor::MoveAnchor, line);
    editor->setTextCursor(cursor);
    editor->setFocus();
    editor->ensureCursorVisible();
}

void MainWindow::onOutlineAnchorNavigate(const QString &anchor)
{
    if (!m_preview || anchor.isEmpty()) {
        return;
    }
    // 预览区标题都带有 id="<anchor>"，scrollToAnchor 可直接定位，
    // 这样即使编辑器被隐藏（预览专属模式）也能在预览里跳转。
    m_preview->scrollToAnchor(anchor);
}

/**
 * 预览区锚点点击 -> 分派处理。
 *
 * 两种锚点：
 *   #<heading-anchor>  —— [TOC] 目录项，跳转到对应标题
 *   #md-task:<line>    —— 任务列表勾选框，切换 [ ] / [x]
 */
void MainWindow::onPreviewAnchorClicked(const QUrl &url)
{
    if (!url.isValid() || !url.hasFragment()) {
        return;
    }
    const QString fragment = url.fragment();
    if (fragment.isEmpty()) {
        return;
    }

    // 任务列表勾选框
    if (fragment.startsWith(QLatin1String("md-task:"))) {
        bool ok = false;
        const int line = fragment.mid(8).toInt(&ok);
        if (ok) {
            toggleTaskAtLine(line);
        }
        return;
    }

    // [TOC] 目录项：跳转到对应标题
    QPlainTextEdit *editor = currentEditor();
    if (!editor) {
        return;
    }

    // 标题扫描与锚点生成规则都来自 DocModel ——
    // 与预览区渲染、大纲面板用的是同一份实现，锚点必然对得上。
    QList<int> lines, levels;
    QList<QString> texts;
    DocModel::collectHeadings(editor->toPlainText(), lines, levels, texts);

    QMap<QString, int> used;
    for (int i = 0; i < texts.size(); ++i) {
        const QString anchor =
            DocModel::uniqueAnchor(DocModel::anchorBase(texts.at(i)), used);
        if (anchor == fragment) {
            onOutlineNavigate(lines.at(i));
            return;
        }
    }
}

/**
 * 切换指定源码行的任务状态（[ ] <-> [x]）。
 */
void MainWindow::toggleTaskAtLine(int line)
{
    QPlainTextEdit *editor = currentEditor();
    if (!editor) {
        return;
    }

    QStringList lines = editor->toPlainText().split('\n');
    if (line < 0 || line >= lines.size()) {
        return;
    }

    QRegularExpression taskRx("^(\\s*)([-*+])\\s+\\[([ xX])\\](\\s+)(.*)$");
    QRegularExpressionMatch m = taskRx.match(lines.at(line));
    if (!m.hasMatch()) {
        return;
    }

    const bool wasChecked = m.captured(3).toLower() == "x";
    const QString newMark = wasChecked ? QStringLiteral(" ") : QStringLiteral("x");

    QString updated = lines.at(line);
    updated.replace(taskRx, QStringLiteral("\\1\\2 [%4]\\5%6")
                        .arg(newMark)
                        .arg(m.captured(4))
                        .arg(m.captured(5)));
    lines[line] = updated;

    // 记录光标位置，替换后恢复
    const int oldPos = editor->textCursor().position();

    editor->setPlainText(lines.join('\n'));

    // 恢复光标到同一行的同一列
    int pos = 0;
    for (int k = 0; k < line; ++k) {
        pos += lines.at(k).length() + 1;
    }
    pos += qMin(oldPos - 0, lines.at(line).length());
    QTextCursor restore(editor->document());
    restore.setPosition(qBound(0, pos, editor->document()->characterCount() - 1));
    editor->setTextCursor(restore);

    statusBar()->showMessage(wasChecked ? tr("任务已标记为未完成")
                                        : tr("任务已标记为完成"), 1500);
}

/**
 * 编辑器光标移动 -> 高亮大纲中的当前章节。
 */
void MainWindow::onEditorCursorMoved()
{
    // 打字机模式：光标始终垂直居中
    if (m_typewriterMode) {
        centerCursorVertically();
    }
    if (!m_outlinePanel) {
        return;
    }
    QPlainTextEdit *editor = qobject_cast<QPlainTextEdit*>(sender());
    if (!editor) {
        return;
    }
    const int line = editor->textCursor().blockNumber();
    m_outlinePanel->setCurrentLine(line);
}

/**
 * 刷新大纲（内容变化时调用）。
 */
void MainWindow::refreshOutline()
{
    if (!m_outlinePanel) {
        return;
    }
    QPlainTextEdit *editor = currentEditor();
    if (!editor) {
        m_outlinePanel->clear();
        return;
    }

    // 内容未变则跳过重扫。
    // 打开文件时 refreshOutline 会被触发多次（newFile、openFile、防抖定时器），
    // 每次都是全量扫标题 + 重建整棵树。
    if (editor == m_outlineCacheEditor && m_contentVersion == m_outlineCacheVersion) {
        return;
    }
    m_outlineCacheEditor = editor;
    m_outlineCacheVersion = m_contentVersion;

    m_outlinePanel->updateOutline(currentTextCached());
}

/**
 * 智能标点开关。
 */
void MainWindow::onSmartPunctuationToggled(bool enabled)
{
    if (m_smartPunctuation) {
        m_smartPunctuation->setEnabled(enabled);
    }
    statusBar()->showMessage(enabled ? tr("智能标点已开启") : tr("智能标点已关闭"), 2000);
}


// ===========================================================================
// Typora 对齐实现
// ===========================================================================

/**
 * 取当前编辑器的全文与光标位置。
 */
QString MainWindow::currentText() const
{
    QPlainTextEdit *editor = currentEditor();
    return editor ? editor->toPlainText() : QString();
}

int MainWindow::currentCursorPos() const
{
    QPlainTextEdit *editor = currentEditor();
    return editor ? editor->textCursor().position() : 0;
}

/**
 * 应用块级编辑结果到编辑器。
 */
void MainWindow::applyBlockResult(const BlockEditor::Result &r)
{
    if (!r.changed) {
        return;
    }
    QPlainTextEdit *editor = currentEditor();
    if (!editor) {
        return;
    }
    QTextCursor cursor = editor->textCursor();
    const int anchor = cursor.anchor();
    const int pos = cursor.position();

    editor->setPlainText(r.text);
    // 恢复选区
    QTextCursor nc(editor->document());
    const int maxPos = editor->document()->characterCount() - 1;
    nc.setPosition(qBound(0, r.selStart, maxPos));
    if (r.selEnd > r.selStart) {
        nc.setPosition(qBound(0, r.selEnd, maxPos), QTextCursor::KeepAnchor);
    }
    editor->setTextCursor(nc);
    (void)anchor;
    (void)pos;
}

/**
 * 通用行内格式切换。
 *
 * 有选区：包裹 / 取消包裹；无选区：插入带占位符的模板。
 */


/**
 * 插入指定级别的标题。
 */


/**
 * 切换当前行的块类型（段落 / 引用 / 列表 / 任务列表 / 代码块 / 公式块）。
 */


/**
 * 插入脚注（正文引用 + 文末定义）。
 */


/**
 * 插入内容目录 [TOC]。
 */


/**
 * 插入 YAML Front Matter。
 */


/**
 * 插入行内公式。
 */


/**
 * 在当前行上方插入空段落。
 */


/**
 * 在当前行下方插入空段落。
 */


/**
 * 切换任务状态（[ ] <-> [x]）。
 */


/**
 * 增加列表缩进。
 */


/**
 * 减少列表缩进。
 */


/**
 * 在表格中插入行。
 */


/**
 * 删除表格当前行。
 */


/**
 * 在表格中插入列。
 */


/**
 * 删除表格当前列。
 */


/**
 * 清除所选文本的行内格式。
 */


/**
 * 复制为 Markdown 源码。
 */


/**
 * 复制为 HTML 代码。
 */


/**
 * 以纯文本粘贴（去除剪贴板中的 HTML 格式）。
 */


/**
 * 以 Markdown 粘贴。
 */


/**
 * 删除选区（无选区时删除后一个字符）。
 */


/**
 * 偏好设置对话框。
 */


/**
 * 打印对话框。
 */


/**
 * 窗口置顶。
 */
void MainWindow::toggleAlwaysOnTop(bool enabled)
{
    if (enabled) {
        setWindowFlag(Qt::WindowStaysOnTopHint, true);
    } else {
        setWindowFlag(Qt::WindowStaysOnTopHint, false);
    }
    show();
}

/**
 * 开发者工具（Qt 原生调试窗口）。
 */


/**
 * 打字机模式：光标始终垂直居中。
 */
void MainWindow::setTypewriterMode(bool enabled)
{
    m_typewriterMode = enabled;
    m_typewriterModeAction->setChecked(enabled);

    if (enabled) {
        centerCursorVertically();
    }
    statusBar()->showMessage(enabled ? tr("打字机模式已开启") : tr("打字机模式已关闭"), 2000);
}

/**
 * 把光标所在行垂直居中显示。
 */
void MainWindow::centerCursorVertically()
{
    QPlainTextEdit *editor = currentEditor();
    if (!editor) {
        return;
    }
    // QPlainTextEdit 自带 centerCursor()。
    // 之前手动算「光标矩形中心 - 视口中心」再改滚动条，
    // 在行高不一致的文档里会积累偏差（代码块、标题行高不同），
    // 内置实现是按块布局精确计算的。
    editor->centerCursor();
}

/**
 * 切换侧边栏页签。
 * 0=文件树 1=大纲 2=文档树
 */
void MainWindow::setSidebarTab(int index)
{
    if (!m_sidebarTabs) {
        return;
    }
    const int count = m_sidebarTabs->count();
    if (index < 0 || index >= count) {
        return;
    }
    m_sidebarTabs->setCurrentIndex(index);
    m_sidebarTabIndex = index;
    if (m_fileTreeDock && m_sidebarVisible) {
        m_fileTreeDock->setVisible(true);
    }

    // 同步勾选状态（大纲已独立成 dock，这里只同步文件树 / 文档树页）
    m_docTreeAction->setChecked(index == 1);
    m_fileTreeAction->setChecked(index == 0);
}


/**
 * 刷新「文档树」页签：列出当前打开的所有文档。
 *
 * 对应 Typora 的「文档树」面板（视图 > 文档树 Ctrl+Shift+2）。
 */
void MainWindow::refreshDocTree()
{
    if (!m_docTree) {
        return;
    }
    m_docTree->clear();

    for (int i = 0; i < m_editorTabs->count(); ++i) {
        QPlainTextEdit *editor = qobject_cast<QPlainTextEdit*>(m_editorTabs->widget(i));
        if (!editor) {
            continue;
        }
        const QString path = editor->property("filePath").toString();
        QString label = m_editorTabs->tabText(i);
        if (label.endsWith("*")) {
            label.chop(1);
        }
        if (path.isEmpty()) {
            label = tr("未命名 %1").arg(i + 1);
        } else {
            label = QFileInfo(path).fileName();
        }

        QTreeWidgetItem *item = new QTreeWidgetItem(m_docTree);
        item->setText(0, label);
        item->setData(0, Qt::UserRole, i);
        item->setToolTip(0, path.isEmpty() ? label : path);

        // 当前文档高亮
        if (i == m_editorTabs->currentIndex()) {
            item->setForeground(0, Theme::accentColor(m_themeId));
        }
    }
    m_docTree->expandAll();
}


/**
 * Typora UI 自检。
 *
 * 覆盖：菜单结构、侧边栏页签、快捷键、行内格式引擎、块级编辑引擎、打字机模式。
 * 输出 KEY=VALUE 形式，供 tools/selftest_typora_ui.py 断言。
 */



// ===========================================================================
// 多格式导出
// ===========================================================================

/**
 * 统一导出入口。
 *
 * 按 Exporter::formats() 里的 id 分发：
 *   - 文本类（markdown/plaintext/latex/rtf/opml/wiki/rst/textile/html/docx/epub）
 *     由 Exporter::generateText 生成字节流后写盘
 *   - 需要打印/渲染上下文的（pdf/odt/image）单独处理
 */


/**
 * 导出 PDF 到指定路径。
 */


/**
 * 导出 ODT（OpenDocument 文本）。
 *
 * 用 Qt 内建的 QTextDocumentWriter("ODF")，它能正确写出 odt 容器。
 */


/**
 * 导出为图像（按扩展名决定 PNG / JPEG）。
 *
 * 做法：把预览用的 QTextDocument 以固定宽度排版，再渲染到 QImage。
 */



/**
 * 自检：把所有导出格式写到指定目录。
 *
 * 输出格式：每行 "格式id|文件路径|字节数|成功?"
 * 供 tools/selftest_export.py 用 zipfile / xml 解析器做真实性校验。
 */



/**
 * 仅用于截图展示：把「文件 > 导出」子菜单弹出来。
 *
 * 用 QMenu::popup 而不是 exec —— popup 是非阻塞的，
 * 截图脚本可以在外部抓到菜单，程序也不会卡住。
 */



// ===========================================================================
// 性能基准打点
// ===========================================================================

/**
 * 开始一次基准计时。
 *
 * 记录的是「累计毫秒」而不是每段耗时，脚本侧再算相邻差值 ——
 * 这样即使某段被跳过（比如文件已打开提前返回），差值仍然正确。
 */





/**
 * 更新窗口标题为「文件名 - Markdown Editor」。
 *
 * 与 Typora 的做法一致（"文件名 - Typora"），
 * 让用户在任务栏/标题栏就能看出当前打开的是哪个文件。
 */
void MainWindow::updateWindowTitle()
{
    QPlainTextEdit *editor = currentEditor();
    QString name = tr("未命名");
    bool modified = false;

    if (editor) {
        const QString path = editor->property("filePath").toString();
        if (!path.isEmpty()) {
            name = QFileInfo(path).fileName();
        }
        modified = editor->property("modified").toBool();
    }

    setWindowTitle(QStringLiteral("%1%2 - Markdown Editor")
                       .arg(name, modified ? QStringLiteral("*") : QString()));
}
