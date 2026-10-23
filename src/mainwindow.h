#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QElapsedTimer>
#include <QAction>
#include <QActionGroup>
#include <QMenuBar>
#include <QToolBar>
#include <QStatusBar>
#include <QTimer>
#include <QSplitter>
#include <QPlainTextEdit>
#include <QTreeView>
#include <QFileSystemModel>
#include <QTextBrowser>
#include <QTabWidget>
#include <QComboBox>
#include <QLabel>
#include <QSpinBox>
#include <QSlider>
#include <QDockWidget>
#include <QDragEnterEvent>
#include <QResizeEvent>
#include <QDropEvent>
#include <QMap>
#include <QTreeWidget>

#include "markdownhighlighter.h"
#include "filemanager.h"
#include "searchreplace.h"
#include "outlinepanel.h"
#include "smartpunctuation.h"
#include "theme.h"
#include "inlineformatter.h"
#include "blockeditor.h"
#include "exporter.h"
#include "docmodel.h"
#include "htmlrenderer.h"

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

    // 启动时打开指定文件（供命令行参数调用）
    void openFileAtStartup(const QString &filePath);

    // 启动时设置深色模式（供命令行 --dark 参数调用）
    void setDarkModeAtStartup(bool enabled);

    // 启动时进入单独预览模式（供命令行 --preview-only 调用）
    void setPreviewOnlyModeAtStartup(bool enabled);

    // 仅供截图：把「文件 > 导出」子菜单弹出来
    void showExportMenuForScreenshot();

    // ---- 性能基准（--bench-open） ----
    void setBenchEnabled(bool on) { m_benchMode = on; }
    QStringList benchLog() const { return m_benchLog; }

    // 自检：触发「单独预览模式」菜单项，验证 action 连接与状态切换
    // 返回切换后的状态描述
    QString selfTestPreviewOnlyToggle();

    // 自检：Typora UI 规格（菜单/快捷键/格式引擎/块级引擎）
    QString selfTestTyporaUi();

    // 自检：把所有导出格式写到指定目录，返回 KEY=VALUE 报告
    QStringList selfTestExportAll(const QString &outDir);

    // 自检：端到端验证「合并 Markdown」：对话框收集文件 -> 合并 -> 新标签
    QString selfTestMerge();

    // 自检：验证「打开多个文件会生成多个可切换的标签」
    QString selfTestTabs();

    // 应用指定主题（供命令行 --theme 参数调用）
    void applyThemeById(const QString &themeId);

    // 临时应用主题，不写入设置（命令行 --theme 用）
    void applyThemeTemporarily(const QString &themeId);

    // 切换到无界面模式：跳过交互式 UI 副作用（用于命令行导出）
    void setHeadlessMode(bool enabled);


    // 将 Markdown 渲染为完整 HTML 页面（供无界面导出与测试使用）
    QString renderMarkdownToHtml(const QString &markdown);

    // 把 markdown 渲染为带「PDF 侧边栏书签（/Outlines 大纲）」的 PDF 字节流。
    // 内部：QTextDocument 打印到临时文件 -> 读回字节 -> 按文档布局算出每个标题的
    // 页码与页内纵坐标 -> 注入 /Outlines。失败或无可注入标题时退回无书签 PDF。
    QByteArray renderPdfBytes(const QString &markdown);

protected:
    void closeEvent(QCloseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private slots:
    // 文件操作
    void newFile();
    void openFile();
    void openFile(const QString &filePath);
    void openFolder();
    void mergeMarkdown();
    void openMergedContent(const QString &text);
    void saveFile();
    bool saveFileAs();
    void closeTab(int index);
    void exportToHtml();
    void exportToPdf();

    // 统一导出入口：按格式 id 导出（见 Exporter::formats()）
    void exportTo(const QString &formatId);
    void exportToPdfFile(const QString &fileName);
    void exportToOdtFile(const QString &fileName);
    void exportToImageFile(const QString &fileName);
    
    // 编辑操作
    void undo();
    void redo();
    void cut();
    void copy();
    void paste();
    void selectAll();
    void find();
    void replace();
    
    // 格式操作
    void insertBold();
    void insertItalic();
    void insertStrikethrough();
    void insertHeading1();
    void insertHeading2();
    void insertHeading3();
    void insertCode();
    void insertCodeBlock();
    void insertLink();
    void insertImage();
    void insertTable();
    void insertListBullet();
    void insertListNumbered();
    void insertBlockquote();
    void insertHorizontalRule();

    // ---- Typora 对齐：通用格式切换 ----
    void toggleInlineFormat(const QString &id);
    void insertHeading(int level);
    void toggleBlockType(const QString &type);
    void insertFootnote();
    void insertToc();
    void insertYamlFrontMatter();
    void insertInlineMath();
    void insertParagraphAbove();
    void insertParagraphBelow();
    void toggleTaskStatus();
    void indentList();
    void outdentList();
    void insertTableRow(bool above);
    void deleteTableRow();
    void insertTableColumn(bool before);
    void deleteTableColumn();
    void clearFormat();
    void setTypewriterMode(bool enabled);
    void setSidebarTab(int index);
    void copyAsMarkdown();
    void copyAsHtml();
    void pasteAsPlainText();
    void pasteAsMarkdown();
    void deleteSelection();
    void showPreferences();
    void showPrintDialog();
    void toggleAlwaysOnTop(bool enabled);
    void showDevTools();
    
    // 视图操作
    void togglePreview();
    void toggleOutline(bool checked);
    void toggleSourceMode();
    void setPreviewOnlyMode(bool enabled);
    void setSourceMode(bool enabled);
    void onOutlineNavigate(int line);
    void onOutlineAnchorNavigate(const QString &anchor);
    void onPreviewAnchorClicked(const QUrl &url);
    void onEditorCursorMoved();
    void onSmartPunctuationToggled(bool enabled);
    void toggleSidebar();
    void toggleFullscreen();
    void setDarkMode(bool enabled);
    void setFocusMode(bool enabled);
    void updateFontSize(int size);
    
    // 标签页操作
    void onTabChanged(int index);
    void onTextChanged();
    void updatePreview();
    
    // 文件树操作
    void onFileTreeClicked(const QModelIndex &index);
    void onFileTreeDoubleClicked(const QModelIndex &index);
    
    // 状态栏
    void updateStatusBar();
    void onAutoSaveTimer();
    
    // 帮助
    void showAbout();
    void showShortcuts();

private:
    void setupUi();
    void createActions();
    void createMenus();
    void createToolbars();
    void createStatusBar();
    void createDockWidgets();
    void createSearchReplaceDialog();
    
    void loadSettings();
    void saveSettings();
    
    bool confirmCloseTab(int index);
    bool saveCurrentTab();
    
    void wrapSelection(const QString &prefix, const QString &suffix);
    void insertText(const QString &text);
    void insertAtLineStart(const QString &prefix);
    void applyEditorTheme(QPlainTextEdit *editor);
    void applyCurrentTheme();
    void refreshOutline();
    void refreshDocTree();
    void updateWindowTitle();

    // ---- 性能基准打点（内部用） ----
    void benchStart();
    void benchMark(const QString &phase);

    QString currentText() const;
    int currentCursorPos() const;
    // 取当前编辑器的全文（带缓存，预览与大纲共用一份拷贝）
    const QString &currentTextCached();
    void applyBlockResult(const BlockEditor::Result &r);
    // 统计单词数（按空白切分）；状态栏用，内容不变时不重复计算
    static int countWords(const QString &text);
    void centerCursorVertically();
    void toggleTaskAtLine(int line);
    void updatePreviewCentering();
    void applyPreviewCentering();
    QPlainTextEdit *editorAtLine(int line) const;
    
    QString getDefaultCss();
    QString getDefaultJs();
    QString generateHtml(const QString &markdown);
    QString generateHtmlBody(const QString &markdown);
    
    // 核心组件
    QSplitter *m_mainSplitter;
    QTabWidget *m_editorTabs;
    QTreeView *m_fileTree;
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
    bool m_sourceMode;
    // 无界面模式（命令行导出）：跳过所有交互式 UI 副作用
    bool m_headlessMode;
    
    // 编辑器组件
    QPlainTextEdit* currentEditor() const;
    MarkdownHighlighter* currentHighlighter() const;
    
    // 文件管理
    FileManager *m_fileManager;
    
    // 搜索替换
    SearchReplaceDialog *m_searchDialog;
    
    // 状态
    bool m_previewVisible;
    bool m_sidebarVisible;
    bool m_darkMode;
    bool m_focusMode;
    // 单独预览模式：隐藏编辑器与侧边栏，只保留预览区（专注阅读）
    bool m_previewOnlyMode;
    int m_fontSize;
    QString m_currentFolder;
    
    // 行内格式 / 块级编辑引擎
    InlineFormatter *m_inlineFormatter;
    BlockEditor *m_blockEditor;

    // ---- 内容版本号与三处缓存 ----
    //
    // 内容版本号由 QPlainTextEdit::textChanged 驱动，任何编辑器的任何改动
    // 都会自增。预览、大纲、全文这三份缓存都以「编辑器 + 版本号」为键。
    //
    // 早期判断「内容有没有变」的办法是把全文 toPlainText() 出来再比对，
    // 这件事本身就要拷贝整份文档 —— 5MB 文档等于每次 10MB 分配，
    // 比重新渲染还贵。而文档真正的 revision 在 setPlainText 时会被重置回
    // 很小的值（clear() 把 revision 置为 -1），不能单独作为判据。
    quint64 m_contentVersion = 0;

    // 当前编辑器的全文缓存（预览、大纲共用一份拷贝）
    QPlainTextEdit *m_textCacheEditor = nullptr;
    quint64 m_textCacheVersion = 0;
    QString m_textCache;

    // 预览渲染缓存
    QPlainTextEdit *m_previewCacheEditor = nullptr;
    quint64 m_previewCacheVersion = 0;

    // 大纲刷新缓存
    QPlainTextEdit *m_outlineCacheEditor = nullptr;
    quint64 m_outlineCacheVersion = 0;

    // 状态栏统计缓存。
    // 单词数没有 O(1) 的算法，只能在内容变化时重算一次；
    // 字符数与行数直接取 QTextDocument 的属性（O(1)）。
    QPlainTextEdit *m_lastStatusEditor = nullptr;
    int m_lastStatusRevision = -1;
    int m_lastStatusWords = 0;

    // ---- 性能基准（--bench-open） ----
    bool m_benchMode;
    QElapsedTimer m_benchTimer;
    QStringList m_benchLog;

    // 打字机模式
    bool m_typewriterMode;

    // 侧边栏当前页签（0=文件树 1=大纲 2=文档树）
    int m_sidebarTabIndex;
    // 文档树（已打开文档列表）
    QTreeWidget *m_docTree;

    // 计时器
    QTimer *m_autoSaveTimer;
    QTimer *m_previewUpdateTimer;
    QTimer *m_statusUpdateTimer;
    QTimer *m_outlineTimer;
    
    // 动作
    QAction *m_newAction;
    QAction *m_openAction;
    QAction *m_openFolderAction;
    QAction *m_mergeAction;
    QAction *m_saveAction;
    QAction *m_saveAsAction;
    QAction *m_closeTabAction;
    QAction *m_exportHtmlAction;
    QAction *m_exportPdfAction;
    QAction *m_exitAction;
    
    QAction *m_undoAction;
    QAction *m_redoAction;
    QAction *m_cutAction;
    QAction *m_copyAction;
    QAction *m_pasteAction;
    QAction *m_findAction;
    QAction *m_replaceAction;
    
    QAction *m_boldAction;
    QAction *m_italicAction;
    QAction *m_strikethroughAction;
    QAction *m_heading1Action;
    QAction *m_heading2Action;
    QAction *m_heading3Action;
    QAction *m_codeAction;
    QAction *m_codeBlockAction;
    QAction *m_linkAction;
    QAction *m_imageAction;
    QAction *m_tableAction;
    QAction *m_bulletListAction;
    QAction *m_numberedListAction;
    QAction *m_blockquoteAction;
    QAction *m_horizontalRuleAction;
    
    QAction *m_togglePreviewAction;
    QAction *m_toggleSidebarAction;
    QAction *m_fullscreenAction;
    QAction *m_darkModeAction;
    QAction *m_focusModeAction;
    
    QAction *m_zoomInAction;
    QAction *m_zoomOutAction;
    QAction *m_zoomResetAction;

    // Typora 风格功能
    QAction *m_toggleOutlineAction;
    QAction *m_toggleSourceModeAction;
    QAction *m_smartPunctuationAction;
    QAction *m_previewOnlyAction;

    // ---- Typora 对齐新增：文件 ----
    QAction *m_newWindowAction;
    QAction *m_printAction;
    QAction *m_preferencesAction;
    QAction *m_preferencesFileAction;
    QMenu   *m_exportMenu;   // 文件 > 导出 子菜单

    // ---- Typora 对齐新增：编辑 ----
    QAction *m_pasteMarkdownAction;
    QAction *m_selectAllAction;
    QAction *m_copyMarkdownAction;
    QAction *m_copyHtmlAction;
    QAction *m_pastePlainTextAction;
    QAction *m_deleteAction;
    QAction *m_mathToolsAction;
    QAction *m_smartQuotesAction;
    QAction *m_smartDashesAction;
    QAction *m_unicodePunctuationAction;

    // ---- Typora 对齐新增：段落 ----
    QAction *m_heading4Action;
    QAction *m_heading5Action;
    QAction *m_heading6Action;
    QAction *m_paragraphAction;
    QAction *m_increaseHeadingAction;
    QAction *m_decreaseHeadingAction;
    QAction *m_tableBlockAction;
    QAction *m_mathBlockAction;
    QAction *m_inlineCodeAction;
    QAction *m_taskListAction;
    QAction *m_taskStatusAction;
    QAction *m_indentAction;
    QAction *m_outdentAction;
    QAction *m_tableRowAction;
    QAction *m_tableColumnAction;
    QAction *m_insertParagraphAboveAction;
    QAction *m_insertParagraphBelowAction;
    QAction *m_inlineMathAction;
    QAction *m_footnoteAction;
    QAction *m_tocAction;
    QAction *m_yamlFrontMatterAction;

    // ---- Typora 对齐新增：格式 ----
    QAction *m_highlightAction;
    QAction *m_superscriptAction;
    QAction *m_subscriptAction;
    QAction *m_commentAction;
    QAction *m_hyperlinkAction;
    QAction *m_createFootnoteAction;
    QAction *m_clearFormatAction;

    // ---- Typora 对齐新增：视图 ----
    QAction *m_outlineAction;
    QAction *m_docTreeAction;
    QAction *m_fileTreeAction;
    QAction *m_sourceModeAction;
    QAction *m_typewriterModeAction;
    QAction *m_showSourceAction;
    QAction *m_sidebarAction;
    QAction *m_alwaysOnTopAction;
    QAction *m_actualSizeAction;
    QAction *m_devToolsAction;
    
    QAction *m_aboutAction;
    QAction *m_shortcutsAction;
};

#endif // MAINWINDOW_H