# Markdown Editor — Windows 原生版

一个用 **Qt 6 + C++17** 写的 Windows 原生 Markdown 编辑器。
界面与操作习惯参考 Typora，但内核完全自研：不依赖 WebEngine、不依赖 Electron，
启动快、内存占用低，5 MB 的大文档也能正常打开。

已用 Qt 6.11.2 + MinGW 13.1 完整编译、实机运行验证通过。

**功能对齐阶段**：参考 Typora 的公开界面规格，移植了 6 项风格功能，
详见「五、Typora 风格功能移植」。

**界面细节阶段**：对齐了 Typora 的**菜单结构（7 菜单）、快捷键表、段落/格式操作全集**，
新增行内格式引擎、块级编辑引擎、文档树页签、打字机模式。
详见「六、Typora 界面对齐」。

**第五阶段（2026-10，可维护性与正确性）**：在引入版本控制之后做了一轮系统性复查，
修掉了若干**用户能直接看到**的问题，并补上了缺失的工程基础设施：

| 类别 | 内容 |
|---|---|
| 一致性 | 预览区与导出格式此前各有各的 Markdown 解析器，对同一文档给出不同结果（表格多空列、行内代码里的 `**` 变真粗体、`~~~` 代码块失效…）。现在预览也走 `DocModel::parse()`，详见 7.3 |
| 功能缺陷 | 源码模式从未真正停用高亮；两个「源码模式」勾选项失同步；「显示源代码」会清空撤销栈；Ctrl+W 在编辑器有焦点时不生效；`## C#` 被剥成 `C` |
| 隐蔽缺陷 | 锚点生成的正则非法（PCRE2 不支持 `\uXXXX`），连字符化从未生效；列表标记从未被剥掉；智能破折号/省略号转换吃错了字符 |
| 性能 | 预览与大纲加内容版本缓存，5 MB 文档打开 **3.67 s → 2.52 s（−31%）**，详见 9.9 |
| 工程 | 引入版本控制；`-Wall -Wextra` 固化进工程配置并清零全部警告；补 94 项 Qt6::Test 单元测试（其中 3 项当场查出真实缺陷）；`mainwindow.cpp` 按职责拆成 5 个编译单元 |

---

## 一、快速开始

```powershell
# 一键构建 + 自检（推荐）
# -Test 会依次跑：单元测试(94) -> 快捷键冲突扫描 -> 脚本自检(181)
.\build.ps1 -Clean -Test

# 仅构建
.\build.ps1

# 只跑单元测试
cd build; ctest --output-on-failure

# 运行
.\dist\MarkdownEditor.exe
```

命令行用法：

```powershell
MarkdownEditor.exe 文件.md            # 启动并打开指定文件
MarkdownEditor.exe 文件.md --dark     # 以深色模式启动
MarkdownEditor.exe 文件.md --theme github   # 指定主题（临时生效，不覆盖已保存的偏好）
MarkdownEditor.exe 文件.md --preview-only   # 直接进入单独预览模式
MarkdownEditor.exe 文件.md --export-html 输出.html   # 无界面转换
MarkdownEditor.exe 文件.md --export-pdf  输出.pdf    # 无界面转换
MarkdownEditor.exe --self-test-export 输出目录 文件.md  # 批量导出全部 14 种格式（自检用）
```

图形界面里的导出入口：**文件 > 导出**，子菜单含 14 种格式。

---

## 二、构建环境

| 组件 | 路径 | 版本 |
|---|---|---|
| Qt | `D:\Qt\6.11.2\mingw_64` | 6.11.2 |
| MinGW | `D:\Qt\Tools\mingw1310_64` | GCC 13.1.0 |
| CMake | `D:\Qt\Tools\CMake_64\bin\cmake.exe` | 3.30 |
| Ninja | `D:\Qt\Tools\Ninja\ninja.exe` | 1.12 |
| Python（辅助脚本） | `C:\Users\pxlwj\.workbuddy\binaries\python\versions\3.13.12` | 3.13.12 |

> `build.ps1` 已自动把 `D:\Qt\Tools\mingw1310_64\bin` 注入 PATH。**这一步不能省** ——
> g++ 调用 `cc1plus` 需要 `libstdc++-6.dll` / `libwinpthread-1.dll`，
> 缺失时 CMake 编译器检测会「静默失败」（只报 "compiler is broken"，不给任何编译错误）。
>
> 同理，`build.ps1` 会在编译前 `touch` 全部源文件：Windows 文件系统时间戳精度较低，
> Ninja 可能漏判「同一秒内修改」的文件，导致改动没有真正编进二进制。

---

## 三、第一阶段：Linux → Windows 移植修复的问题

### 3.1 编译阻断类

| # | 问题 | 位置 | 处理 |
|---|---|---|---|
| 1 | `QRegExp` 在 Qt 6 中已移除 | `mainwindow.cpp` `generateHtml()` | 改用 `QRegularExpression` |
| 2 | `find_package(Qt6.5 ...)` 锁死小版本 | `CMakeLists.txt` | 改为 `find_package(Qt6 REQUIRED ...)`，不锁小版本，兼容 6.5~6.11+ |
| 3 | `QWebEnginePage` 依赖未安装模块 | `previewwebpage.cpp` | 该文件本就是死代码（未被 `.pro`/`CMakeLists` 引用），不纳入构建 |
| 4 | `QIcon::fromTheme` 在 Windows 无主题引擎 | 10 处 action | 改为内置 SVG 资源，详见 3.4 |

### 3.2 行为正确性类（**最隐蔽，不修就会静默出错**）

| # | 问题 | 后果 | 处理 |
|---|---|---|---|
| 5 | `QRegExp` 的 `^ $` 默认按**行**匹配，`QRegularExpression` 默认按**整串**匹配 | 标题、列表、引用、水平线在预览区**全部渲染不出来** | 所有行锚定正则加 `QRegularExpression::MultilineOption` |
| 6 | `generateHtml()` 用 `while (rx.indexIn(...)) { html.replace(rx.cap(0), ...) }` | 替换后的标签会被后续规则重复匹配，存在死循环与标签嵌套风险 | 重写为**逐行状态机解析器** |
| 7 | `generateHtml()` 末尾对整个 HTML 执行 `replace("&", "&amp;")` | 双重转义，`&amp;` 变 `&amp;amp;` | 改为在**行内元素处理阶段**按需转义 |
| 8 | 段落 `<p>` 包裹逻辑会吞掉已生成的 `<h1>`/`<pre>`/`<ul>` 标签 | 结构错乱 | 状态机按块级结构切分，段落独立处理 |
| 9 | 3 组快捷键冲突 | Windows 上真实冲突 | 见下表 |
| 10 | 深色模式只改预览区，编辑器仍白底 | 深色模式观感割裂 | 新增 `applyEditorTheme()`，编辑器同步换肤 |
| 11 | `insertImage()` 把 Windows 反斜杠路径直接拼进 Markdown | 图片链接失效 | 统一转正斜杠（URL 风格） |
| 12 | `QTextStream` 未显式指定编码 | 区域设置影响读写结果 | 显式 `setEncoding(QStringConverter::Utf8)` |
| 13 | `installEventFilter(this)` 但未实现 `dragEnterEvent`/`dropEvent` | 从资源管理器拖入 .md 无反应 | 补齐拖放打开文件 |
| 14 | **文件树从未设置 `setRootPath`** | 文件树永远是空的 | 启动时用当前目录初始化，打开文件时定位到该文件目录 |

**修复的快捷键冲突：**

| 快捷键 | 原绑定 | 冲突对象 | 现绑定 |
|---|---|---|---|
| `Ctrl+Shift+O` | 打开文件夹 | 有序列表 | 有序列表保留，打开文件夹 → `Ctrl+Shift+L` |
| `Ctrl+Shift+S` | 删除线 | 另存为 | 另存为保留，删除线 → `Ctrl+Shift+X` |
| `Ctrl+B` | 显示/隐藏侧边栏 | 粗体 | 粗体保留，侧边栏 → `Ctrl+Shift+B` |
| `Ctrl+Q` | 引用 | 退出 | 退出保留，引用 → `Ctrl+Shift+Q` |
| `Ctrl+F` | 查找和替换 | 查找 | 查找保留，替换 → `Ctrl+R` |
| `Ctrl+Shift+O` | 大纲面板 | 有序列表 | 大纲 → `Ctrl+Shift+N` |

### 3.3 Windows 体验类

- **图标**：原代码 37 个图标位中只有 10 个有图标，且全部依赖 `QIcon::fromTheme`（Linux 的 XDG/GTK 机制）。Windows 上会**静默退化成空白按钮**。现生成 39 个内置 SVG 图标（`tools/gen_icons.py`），格式类工具栏（粗体/斜体/标题/代码/链接/图片/表格/列表/引用）全部补齐图标。
- **字体**：编辑器字体加 `setStyleHint(QFont::Monospace)` + `setFixedPitch(true)` 回退链，Windows 用 Consolas，Linux 回退系统等宽字体。
- **窗口图标**：`setWindowIcon(QIcon(":/icons/app.svg"))`，任务栏与标题栏均显示。
- **命令行打开文件**：新增 `openFileAtStartup()`，支持资源管理器双击 .md 文件关联。
- **拖放打开文件**：补齐 `dragEnterEvent` / `dropEvent`。

### 3.4 第一阶段新增能力

| 功能 | 说明 |
|---|---|
| `--dark` | 启动时深色模式 |
| `--export-html` | 无界面 Markdown → HTML（可脚本化批处理） |
| `--export-pdf` | 无界面 Markdown → PDF（`QPrinter::PdfFormat`，不依赖已装打印机） |
| 渲染自检 | `tools/selftest_render.py`，16 项 HTML 结构断言 |
| 快捷键检查 | `tools/check_shortcuts.py`，扫描重复绑定 |

---

## 四、功能取舍依据

功能范围按 Typora 的**公开使用体验**确定：装一份试用版，把菜单点一遍、
快捷键按一遍，记下「哪些操作是日常高频的」，再决定本项目实现哪些。
没有任何逆向工程，也不复用它的代码或资源 —— 本项目的解析器、
渲染器、导出器全部从零实现。

| 参考的能力 | 本项目的实现方式 |
|---|---|
| 7 个菜单的分组与顺序 | 按公开界面重新组织，快捷键表自行设计并做冲突扫描 |
| 大纲面板 / 目录块 / 任务列表交互 | 基于自研 IR（`docmodel`）实现，非移植 |
| 多主题与 CSS 变量模型 | 自研 `theme.cpp`，7 套主题配色全部自己写 |
| 智能标点（弯引号 / 破折号） | 自研规则引擎，含「英文缩写不误转」等边界处理 |
| 打字机模式 / 专注模式 / 源码模式 | 基于 Qt 原生能力实现 |

### 未实现的能力及原因

| 能力 | 未实现原因 |
|---|---|
| Mermaid 图表 | 需要 JS 引擎执行；本项目刻意不引入 WebEngine，保持原生轻量 |
| MathJax 公式渲染 | 同上。公式块可正常解析、导出为 LaTeX，只是预览区不做实时渲染 |
| Pandoc 多格式导出 | 需要外部 Pandoc 二进制；本项目 14 种导出格式全部自研，无外部依赖 |
| 拼写检查 | 需要引入约 40 MB 的词典与原生库，与「轻量」目标冲突 |
| 所见即所得即时渲染 | 需要在编辑器内核上做深度改造，工作量与风险远高于收益 |

---

## 五、功能移植

参考 Typora 的使用体验，实现了 7 项高价值功能（全部为自研实现）：

| # | 功能 | 来源 | 本项目实现 |
|---|---|---|---|
| 1 | **大纲面板** | Typora 的 Outline 侧栏 | `src/outlinepanel.cpp`：QTreeWidget 树状大纲，随内容实时刷新（300 ms 防抖）、光标移动高亮当前章节、点击跳转 |
| 2 | **TOC 目录块** | Typora 的 `[TOC]` 语法 | 标题自动生成唯一锚点 `id`；`[TOC]` 渲染为可点击目录，点击跳转对应标题 |
| 3 | **任务列表交互** | Typora 的勾选框交互 | 预览区用 `☐`/`☑` 字形 + `#md-task:<行号>` 锚点，点击回写源码的 `[ ]`/`[x]` |
| 4 | **多主题系统** | Typora 的主题切换 | `src/theme.cpp`：7 套主题（默认/GitHub/GitHub 深色/Night/Newsprint/Pixyll/Whitey），输出 CSS 变量 + 完整预览样式 |
| 5 | **智能标点** | Typora 的「编辑 > 智能标点」 | `src/smartpunctuation.cpp`：直引号→弯引号、`--`→en dash、`---`→em dash、`...`→省略号；英文缩写场景（don't）不误转 |
| 6 | **单独预览模式** | 本项目自研 | `Ctrl+Shift+P` 切换：隐藏编辑器与侧边栏，预览区独占中央区域并居中留白；与专注模式、源码模式互斥 |
| 7 | **源码模式** | Typora 的「视图 > 源代码模式」 | `Ctrl+Shift+M` 切换，隐藏预览区 |

**主题与深色模式相互独立**（与 Typora 一致）：用户显式选了 GitHub 主题，就不会被「深色模式」开关改成 GitHub 深色；深色模式只影响编辑器的高亮配色与背景。

**主题持久化**：通过 `QSettings` 保存选择，重启后恢复。

### 单独预览模式（第三阶段新增）

`Ctrl+Shift+P` 或「视图 > 单独预览模式」切换：

- 隐藏编辑器、隐藏侧边栏（文件树 / 大纲），预览区独占中央区域
- 正文**居中留白**，宽度按 860px 舒适阅读宽度计算（不小于主题自带的 contentWidth）
- 菜单栏、工具栏、状态栏**保留** —— 否则无法退出该模式
- 与专注模式、源码模式**互斥**：进入前自动退出冲突模式
- 再次触发完全还原（编辑器、侧边栏、分割比例）

命令行也支持：`--preview-only`（演示 / 截图 / 全屏阅读）。

### 三种视图模式的区别

| 模式 | 快捷键 | 编辑器 | 预览区 | 侧边栏 | 工具栏/菜单 | 用途 |
|---|---|---|---|---|---|---|
| 常规 | — | 显示 | 显示 | 显示 | 显示 | 边写边看 |
| 专注模式 | `Ctrl+Shift+F` | 显示 | 隐藏 | 隐藏 | 隐藏 | 纯写作，不被打扰 |
| 源码模式 | `Ctrl+Shift+M` | 显示（纯源码） | 隐藏 | 显示 | 显示 | 看原始 Markdown |
| **单独预览** | `Ctrl+Shift+P` | 隐藏 | 显示（居中） | 隐藏 | 显示 | 专注阅读 / 演示 |

### 新增命令行参数

```powershell
--theme <id>       # default/github/github-dark/night/newsprint/pixyll/whitey
--dump-outline     # 输出文档大纲（层级/行号/锚点），用于脚本化处理
```

### 移植过程中踩到的 Qt 坑

| 坑 | 现象 | 解法 |
|---|---|---|
| **`QVariantList` 嵌套歧义** | `QVariant` 有 `QVariant(const QList<QVariant>&)` 构造重载，`QList<QVariant>::append(QList<QVariant>)` 会把整个列表包成**单个** QVariant，字段错位（level 变成 19968 = "一"的码点） | 改用明确的 `struct HeadingInfo` 结构体传递 |
| **`QTreeWidgetItemIterator` 空树崩溃** | `for (... it(m_tree); *it; ++it)` 在树为空时 `operator*()` 解引用空指针 → SIGSEGV | 改用 `it.operator*() != nullptr` 判空（Qt6 无 `operator!=` / `end()`） |
| **懒创建 QTimer 崩溃** | 在 `onTextChanged`（由 `setText()` 触发）里 `new QTimer(this)`，在信号发射过程中创建对象导致崩溃 | 计时器一律在构造函数中创建 |
| **Qt QSS 不支持 `QTreeView::item` 的 height** | 设置 `height: 24px` 会让整棵树不可见 | 行高改用 `margin` 表达，字号放 `QTreeWidget` 上 |
| **Ninja 漏判同秒修改** | 改完源码重新构建，Ninja 报 "no work to do"，一直在测旧二进制 | 构建前 `touch` 全部源文件 |
| **QTextBrowser 无法用 CSS 居中** | `margin: 0 auto` 不生效、body 的 `padding` 被忽略、div 的 `margin-left` 也不生效 | 改用 Qt 原生 API：`QTextDocument::rootFrame()->setFrameFormat()` 设左右边距（要在 `setHtml()` 之后调用，setHtml 会重置文档格式） |
| **QSplitter 隐藏子控件后不重新分配** | 隐藏编辑器后预览区仍保持旧宽度 | 配合 `setStretchFactor()` + `setSizes({0, 1})`，并在 `resizeEvent` 中再触发一次 |
| **菜单栏改 tab 结构后 dock 取不到** | `qobject_cast<QDockWidget*>(m_fileTree->parent())` 返回 nullptr（parent 变成 QTabWidget 了），侧边栏显隐失效 | 改用成员指针 `m_fileTreeDock` |
| **沙箱里模拟键盘送不到前台窗口** | Windows 前台锁定策略拒绝 `SetForegroundWindow`，`SendInput` 也无效；Qt 自绘菜单没有原生 HMENU，`WM_COMMAND` 也发不了 | 改用程序内置自检钩子（`--self-test-preview-only`）直接触发 QAction，走与用户点击完全相同的代码路径 |

---

## 六、界面对齐

依据 Typora 的公开界面规格，确认了它的完整菜单与快捷键布局，并据此对齐。

### 6.1 菜单栏：5 个 -> 7 个

Typora 的菜单栏是 **文件 / 编辑 / 段落 / 格式 / 视图 / 主题 / 帮助**，
此前本项目只有 5 个（缺「段落」「主题」独立成菜单）。现已重构，分组与顺序均按上述规格对齐。

| 菜单 | 分组 |
|---|---|
| 文件 | 新建 / 新建窗口 / 打开 / 打开最近文件 / 打开文件夹 → 保存 / 另存为 → 导出 / 打印 / 导出为 PDF → 偏好设置 / 偏好设置文件 → 退出 |
| 编辑 | 撤销 / 重做 → 剪切 / 复制 / 粘贴 / 粘贴为 Markdown / 删除 → 全选 → 复制为 Markdown / 复制为 HTML 代码 / 粘贴为纯文本 → **智能标点（子菜单）** / 选择（子菜单） / 删除（子菜单） / 数学工具（子菜单） → 查找 / 替换 → 偏好设置 |
| 段落 | 一级~六级标题 / 段落 → 提升标题级别 / 降低标题级别 → 表格 / 公式块 / 代码块 / 代码 / 引用 → 有序列表 / 无序列表 / 任务列表 / 任务状态 / 列表缩进 → 表格 / 表格行 / 表格列 → 在上方插入段落 / 在下方插入段落 → 数学公式 / 脚注 / 水平分割线 / 内容目录 / YAML Front Matter |
| 格式 | 加粗 / 斜体 / 删除线 / 代码块 / 代码 → 高亮 / 上标 / 下标 / 注释 → 超链接 / 图像 / 表格 / 创建脚注 → 清除格式 |
| 视图 | 显示/隐藏侧边栏 → 大纲 / 文档树 / 文件树 → 搜索 → 源代码模式 / 专注模式 / 打字机模式 / 显示源代码 → 侧边栏 / 保持窗口在最前端 → 实际大小 / 放大 / 缩小 → 开发者工具 |
| 主题 | Github / Newsprint / Night / Pixyll / Whitey |
| 帮助 | 快捷键 / 关于 |

### 6.2 快捷键：39 -> 73 个，全部对齐 Typora

Typora 的快捷键已全部落实：

| 功能 | Typora 键 | 本项目 |
|---|---|---|
| 一级~六级标题 | `Ctrl+1`~`Ctrl+6` | 同 |
| 段落 | `Ctrl+0` | 同 |
| 提升 / 降低标题级别 | `Ctrl+[` / `Ctrl+]` | 同 |
| 表格 | `Ctrl+Shift+]` | 同 |
| 公式块 | `Ctrl+Shift+M` | 同 |
| 代码块 / 代码 | `Ctrl+Shift+C` / `Ctrl+Shift+I` | 同 |
| 引用 | `Ctrl+Shift+Q` | 同 |
| 有序 / 无序 / 任务列表 | `Ctrl+Shift+U` / `Ctrl+Shift+L` / `Ctrl+Shift+X` | 同 |
| 加粗 / 斜体 / 删除线 | `Ctrl+B` / `Ctrl+I` / `Ctrl+K` | 同 |
| 高亮 | `Alt+Shift+H` | 同 |
| 上标 / 下标 | `Ctrl+Shift++` / `Ctrl+Shift+-` | 同 |
| 超链接 | `Ctrl+Shift+K` | 同 |
| 源代码模式 | `Ctrl+/` | 同 |
| 专注 / 打字机模式 | `F9` / `F10` | 同 |
| 侧边栏 | `F11` | 同 |
| 大纲 / 文档树 / 文件树 | `Ctrl+Shift+1` / `2` / `3` | 同 |
| 实际大小 | `Ctrl+Shift+9` | 同 |
| 导出为 PDF | `Alt+Shift+P` | 同 |
| 开发者工具 | `Shift+F12` | 同 |

**冲突处理**：Typora 自身会在不同菜单里复用同一快捷键（如 `Ctrl+Shift+C` 既是「代码块」
又是「复制为 Markdown」）。本项目是单窗口全局快捷键，必须唯一，
因此按「编辑类 > 视图类 > 格式类」优先级取舍 —— 保留与 Typora 语义直接对应的一方，
另一方改用 Alt/F 组合（如 `Alt+L` 插入链接、`Alt+M` 复制 Markdown、`F4` 切换预览、`F5` 全屏）。

### 6.3 两个新引擎

| 引擎 | 文件 | 职责 |
|---|---|---|
| 行内格式引擎 | `src/inlineformatter.cpp` | 加粗 `**`、斜体 `*`、删除线 `~~`、行内代码 `` ` ``、高亮 `==`、上标 `^`、下标 `~`、注释 `<!-- -->`、行内公式 `$`、超链接 `[](  )` 的**包裹与取消**。有选区则包裹/取消，无选区则插入带占位符的模板 |
| 块级编辑引擎 | `src/blockeditor.cpp` | 标题级别升降、块类型切换（段落/引用/有序列表/无序列表/任务列表/代码块/公式块）、表格行列增删、上下插入段落、脚注/目录/分割线/YAML 插入 |

两者都是**纯函数**（输入全文 + 光标位置，输出新全文 + 新选区），
便于单测，也便于在多处复用。

### 6.4 新增功能

| 功能 | 说明 |
|---|---|
| **文档树页签** | 侧边栏第三页（`Ctrl+Shift+2`），列出当前打开的所有文档，点击切换 |
| **打字机模式** | `F10`，光标始终垂直居中，滚动跟随光标 |
| **偏好设置对话框** | `Ctrl+,`，字号 / 主题 / 深色模式 / 智能标点 / 打字机模式 |
| **智能标点子菜单** | 拆成 4 个可独立开关的项：输入时转换 / 智能引号 / 智能破折号 / Unicode 标点转换 |
| **复制为 Markdown / HTML** | `Alt+M` / `Ctrl+Shift+H` |
| **粘贴为纯文本 / Markdown** | `Ctrl+Shift+T` / `Ctrl+Shift+V` |
| **打印** | `Ctrl+P` |
| **开发者工具** | `Shift+F12`，输出日志 / 对象树 / 属性三个视图 |
| **保持窗口在最前端** | 视图菜单开关 |
| **清除格式** | 去掉选区/当前行的所有行内标记 |

### 6.5 侧边栏三视图

Typora 的侧边栏有「大纲 / 文档树 / 文件树」三种内容，分别对应
`Ctrl+Shift+1` / `2` / `3`。本项目原先只有「文件树 / 大纲」两个 tab，现补齐第三个。

### 6.6 未对齐的一项（技术边界）

**Typora 的「光标处即时渲染」** —— 编辑区不是纯源码，而是光标所在块显示渲染结果、
其余块显示源码，滚动时逐块切换。这需要把渲染结果回写进编辑器 DOM，
本质上是重写整个编辑器内核（Typora 用 CodeMirror 6 + 自研渲染层），
工作量与风险远超本项目的收益。

本项目的取舍：**保持编辑区为纯源码**（这也是 Typora「源代码模式」的行为），
渲染结果由右侧预览区承担，并提供「单独预览模式」用于纯阅读场景。

## 七、多格式导出

Typora 的「导出」是一个含十余种格式的子菜单。本项目原先把导出做成两个平铺的菜单项
（HTML / PDF），现已重构为子菜单并补齐到 **14 种格式**，顺序与 Typora 一致。

### 7.1 支持的格式

| 格式 | 扩展名 | 实现方式 | 校验手段 |
|---|---|---|---|
| HTML | `.html` | 复用 `generateHtml()` | 结构断言 |
| PDF | `.pdf` | `QPrinter` + A4 页边距 | 校验 `%PDF-` 头、`%%EOF` 尾、页数 |
| **Word** | `.docx` | **自研 OOXML + ZIP 容器** | **用 Word 16.0 实际打开验证** |
| OpenOffice | `.odt` | Qt 内建 `QTextDocumentWriter("ODF")` | **用 Word 16.0 实际打开验证** |
| **EPUB** | `.epub` | **自研 EPUB 3 + ZIP 容器** | 校验 mimetype 首位、manifest/spine 一致性、XHTML 良构 |
| LaTeX | `.tex` | 从 IR 生成 | 校验环境配平、中文字体包 |
| RTF | `.rtf` | 从 IR 生成，中文走 `\uN?` 转义 | 校验括号配平、字体表 |
| OPML | `.opml` | 标题层级 → 嵌套 outline | 校验 XML + 标题完整性 |
| Wiki | `.wiki` | MediaWiki 语法 | 中文保真度 |
| reStructuredText | `.rst` | RST 语法 | 中文保真度 |
| Textile | `.textile` | Textile 语法 | 中文保真度 |
| 图像 | `.png` / `.jpg` | `QTextDocument` 渲染到 `QImage` | 校验 PNG 签名与 IHDR |
| Markdown | `.md` | 规范化重排（非原样拷贝） | 中文保真度 100% |
| 纯文本 | `.txt` | 去所有格式标记 | 校验无残留标记 |

### 7.2 为什么自己写 ZIP

docx 和 EPUB 本质都是「ZIP + XML」，但 Qt 没有公开的 ZIP 写入 API
（`QZipWriter` 在 `private/` 头里，不适合依赖）。因此实现了 `src/zipwriter.{h,cpp}`：

- 只用 **STORE（不压缩）** 模式 —— 实现简单、无 deflate 依赖、兼容性最好
- EPUB 规范本身就要求 `mimetype` 必须 stored，所以这个选择对 EPUB 是必须的
- CRC32 自实现（表驱动，多项式 `0xEDB88320`）
- 文件名标记 UTF-8（通用标志位 bit 11），中文文件名不会乱码

### 7.3 中间表示（IR）：一次解析，多格式复用

`src/docmodel.{h,cpp}` 定义了 Markdown 的中间表示：

```
DocInline  — 文本 + bold/italic/code/strike/highlight/sup/sub/math/link/image
DocBlock   — Heading / Paragraph / ListItem / Quote / CodeBlock / Table / Hr
             / MathBlock / RawHtml / Toc
```

`DocModel::parse()` 把 Markdown 解析成 `QList<DocBlock>`，然后
**预览区**（`src/htmlrenderer.{h,cpp}`，IR → HTML）与 10 个格式生成器
（docx / EPUB / XHTML / LaTeX / RTF / OPML / MediaWiki / RST / Textile / Markdown）
全部从同一棵 IR 出发。好处：

- 各格式的语义一致（不会出现 docx 认得出标题、RTF 认不出）
- **预览与导出必然一致**（同一份 IR，不可能对同一文档给出两种解释）
- 行内解析与块级解析各自可单测
- 加新格式只需写一个 `toXxx()` 函数

> **曾经有过两套解析器**。预览区早先自己写了一套正则状态机，
> 与 `DocModel::parse()` 对同一份文档给出不同结果：
> 表格多出首尾两个空列、行内代码里的 `**` 变成真粗体、
> `~~~` 代码块整个失效、嵌套列表被拍平、`***粗斜体***` 多出一个孤立的 `*`……
> 用户看到的就是「预览里一种样子，导出来另一种样子」。
> 现在预览区也走 `DocModel::parse()`，一份文档只允许有一套解析规则。
> 这七条差异都已写成回归断言（`tools/selftest_render.py` 的「预览/导出一致性」一组）。

**注意**：行内解析用「扫描 + 递归」而非正则全局替换 —— 正则难以正确处理
`***粗斜体***` 这类嵌套，且不配对标记容易把整篇文档吃掉。

### 7.4 踩到的坑（都是真实会导致"打不开"的问题）

| 坑 | 现象 | 解法 |
|---|---|---|
| **EPUB 闭合标签带属性** | 生成 `</ul class="tasks">`，XML 直接非法，EPUB 阅读器拒绝加载 | 标签名与属性分开存：`listTag` 只存 `ul`/`ol` |
| **OPC 声明遗漏** | `document.xml` 用了 `numPr` 编号，但 `numbering.xml` 既没在 `[Content_Types].xml` 声明、也没在 `document.xml.rels` 引用 → Word 提示「内容有问题」 | 两处都补齐。这条只有真拿 Word 打开才会暴露 |
| **PNG 体积膨胀 43 倍** | 900px 宽的图导出成 5 MB | `QImage::save` 的 quality 对 PNG 是 **zlib 压缩级别反转**语义：0 = 最高压缩，100 = 几乎不压缩。传 0 后降到 116 KB |
| **PowerShell 调用 GUI 程序不等待** | `& exe --self-test-export` 立刻返回，文件还没生成就去做校验 | 改用 `Start-Process -Wait -PassThru` |
| **Qt 的 `emit` 是关键字宏** | 自定义函数 `void emit(...)` 被宏展开成 `void (...)`，编译报「invalid declarator」 | 改名为 `emitInline` |
| **自检在打开文件之前执行** | 导出的是空文档（`chars=0`），所有文本格式都是 0 字节 | 自检块里先 `openFileAtStartup()` |
| **OPML 的"中文保真"检查不公平** | OPML 是大纲格式只保留标题，全文比对必然是 41% | 改为按「所有标题文字是否都在」校验 |

### 7.5 验证

新增两套自检：

- `tools/selftest_export.py` —— **44 项**容器与结构校验
  （zipfile 解析、XML 良构、mimetype、OPC 声明完整性）
- `tools/selftest_export_deep.py` —— **21 项**深度校验
  （manifest/spine 一致性、PDF 字体嵌入与 CID 编码、样式引用完整性、各格式中文保真度）

**关键验证：用真实的 Microsoft Word 16.0 打开导出的文件**

```
export-docx.docx : OPEN_OK  paragraphs=48  words=290  tables=1  pages=2
                   p1: style=标题 1  outline=1  text=Typora 功能验证文档
                   p5: style=标题 2  outline=2  text=二级标题：功能清单
export-odt.odt   : OPEN_OK  paragraphs=64  words=337  tables=1
```

标题被正确识别为「标题 1/2/3」且 outline 级别正确 —— 说明 Word 的**导航窗格可用**，
不是"能打开但结构丢失"。

## 八、目录结构

```
markdown-editor-win\
├── build.ps1                  一键构建（-Clean 清理 / -Test 自检）
├── build.bat                  cmd 版本（备用）
├── CMakeLists.txt             CMake 构建
├── MarkdownEditor.pro         qmake 构建（备用路径）
├── src/                       移植后的源码
│   ├── main.cpp               命令行解析、深色/主题/导出模式
│   ├── mainwindow.cpp/.h      主窗口（构造、界面搭建、文件操作、视图模式、渲染）
│   ├── mainwindow_format.cpp  格式菜单与段落菜单背后的全部动作
│   ├── mainwindow_export.cpp  14 种导出格式
│   ├── mainwindow_selftest.cpp --self-test-* 命令行自检钩子
│   ├── mainwindow_devtools.cpp 偏好设置、快捷键速查、打印、开发者视图、性能打点
│   ├── theme.cpp/.h           主题系统（7 套主题 + CSS 变量）
│   ├── outlinepanel.cpp/.h    大纲面板
│   ├── smartpunctuation.cpp/.h 智能标点
│   ├── inlineformatter.cpp/.h 行内格式引擎（Typora 格式菜单）
│   ├── blockeditor.cpp/.h     块级编辑引擎（Typora 段落菜单）
│   ├── docmodel.cpp/.h        Markdown 中间表示（IR）+ 解析器（全项目唯一解析入口）
│   ├── htmlrenderer.cpp/.h    IR -> HTML 渲染器（预览区与 HTML 导出共用）
│   ├── exporter.cpp           文本类导出器（LaTeX/RTF/OPML/Wiki/RST/Textile/XHTML/Markdown）
│   ├── exporter_office.cpp    docx / EPUB 容器导出器
│   ├── zipwriter.cpp/.h       最小 ZIP 写入器（docx / EPUB 基础）
│   ├── markdownhighlighter.*  语法高亮（已修 MultilineOption）
│   ├── filemanager.*          文件读写与监视
│   └── searchreplace.*        查找替换
├── tests/                     单元测试（Qt6::Test）
│   ├── CMakeLists.txt
│   ├── tst_docmodel.cpp       Markdown 解析（38 项）
│   ├── tst_blockeditor.cpp    块级编辑引擎（30 项）
│   ├── tst_inlineformatter.cpp 行内格式引擎（10 项）
│   └── tst_smartpunctuation.cpp 智能标点（16 项）
├── resources/
│   ├── icons/                 40 个界面图标 + 2 个应用图标（app-icon*.svg）
│   ├── app.ico                应用图标（7 档尺寸，由 --export-icon 生成）
│   └── icons.qrc              Qt 资源清单（由脚本生成）
├── tools/                     辅助脚本
│   ├── gen_icons.py           生成 SVG 图标 + qrc
│   ├── check_shortcuts.py     快捷键冲突扫描
│   ├── selftest_render.py     渲染断言自检（23 项，含预览/导出一致性回归）
│   ├── selftest_typora.py     Typora 功能自检（18 项）
│   ├── selftest_themes.py     主题自检（7 项）
│   ├── selftest_preview_only.py  单独预览模式自检（11 项）
│   ├── selftest_typora_ui.py  Typora UI 规格自检（57 项）
│   ├── selftest_export.py     导出容器与结构自检（44 项）
│   ├── selftest_export_deep.py  导出深度校验（21 项）
│   ├── verify_ui.py           界面截图验证
│   ├── verify_outline.py      大纲面板截图验证
│   ├── verify_preview_only.py 单独预览模式截图验证
│   ├── verify_export_menu.py  导出菜单截图验证
│   ├── gen_bench_docs.py      生成性能测试文档（200KB/1MB/5MB）
│   ├── check_exe_icon.py      验证 exe 内嵌图标（ExtractIconExW）
│   ├── bench_open_external.py 外部测量打开耗时（可与任意编辑器对比）
│   ├── shot_window.py         PrintWindow 抓窗口图（不依赖前台）
│   └── fix_ps1_encoding.py    ps1 编码规范化
├── benchdata/                 性能测试文档（200KB/1MB/1.5MB/2MB/3MB/5MB，已忽略）
├── testdata/                  测试用 Markdown
├── shots/                     界面截图（已忽略）
├── build/                     CMake 中间产物（已忽略）
└── dist/                      分发目录（可直接拷走使用，已忽略）
    └── out-test/              导出格式自检产物（14 个文件）
```

---

## 九、性能：大文档打开速度

在 5 个规模上实测打开耗时，并与 Typora（1.x，Electron + CodeMirror）对比。

### 9.1 测试方法

**公平性要求**：两边用完全相同的外部测量标准，不依赖各自内部的打点。

| 指标 | 含义 |
|---|---|
| `标题就绪` | 主窗口标题包含目标文件名 —— 文件已读入、窗口已出现 |
| `渲染完成` | 目标进程 CPU 时间连续 700ms 稳定 —— 渲染结束、可交互 |

每个规模跑 3 轮取最优值（OS 调度噪声大，单次测量不可靠）。

测试文档由 `tools/gen_bench_docs.py` 生成（固定随机种子），内容模拟真实技术文档：
标题层级、中文段落、列表、代码块、表格、引用。

### 9.2 结果

| 文档 | 字符 / 行 | 本项目 | 换 QPlainTextEdit 后 | Typora |
|---|---|---|---|---|---|
| 200 KB | 94 K / 3.5 K | 2.00 s | **2.00 s** | 2.42 s |
| 1 MB | 471 K / 17 K | 3.24 s | **2.58 s**（−20%） | 3.10 s |
| 1.5 MB | 708 K / 26 K | 3.20 s | **2.89 s**（−10%） | 3.38 s |
| 2 MB | 940 K / 34 K | 3.62 s | **3.29 s**（−9%） | **拒绝打开** |
| 5 MB | 2.36 M / 86 K | 7.80 s | **6.75 s**（−13%） | **拒绝打开** |

（「本项目」列是 QTextEdit 时期的数据，「换 QPlainTextEdit 后」是当前版本。
两个「本项目」列都指渲染完成时间，测试方法同为外部统一标准。）

**结论**：

1. **200 KB ~ 1.5 MB：两者相当**。本项目在「标题就绪」上稳定快 0.5~1.0 s
   （Qt 原生 + 文件读取直接），渲染阶段互有胜负，总体持平。
2. **2 MB 以上：Typora 直接拒绝**，弹窗提示「该文件过大，因此无法在 Typora 中呈现」，
   本项目仍能完整打开（5 MB 文档大纲 33 章节、编辑器、预览、状态栏统计全部正常）。
3. Typora 的大小上限实测在 **1.5 MB ~ 2 MB 之间**（1.5 MB 可开，2 MB 拒绝）。

### 9.3 优化过程（内部耗时，不含进程启动）

初版在 1 MB 文档上要 2.25 s、5 MB 要 5.54 s。定位到根因后优化：

| 规模 | 优化前 | 优化后 | 提升 |
|---|---|---|---|
| 200 KB | 480 ms | **128 ms** | 3.7× |
| 1 MB | 2249 ms | **622 ms** | 3.6× |
| 5 MB | 5535 ms | **3402 ms** | 1.6× |

**根因一：`QRegularExpression` 在循环内构造（最大问题）**

`generateHtml()` 与 `OutlinePanel::updateOutline()` 里，每处理一行都要现场构造正则对象。
`QRegularExpression` 的构造会**编译整个模式**，成本极高：

- 1 MB / 17 K 行 × 6 个正则 = 10 万次编译
- 5 MB / 86 K 行 × （1 + 2 + 6）个正则 = 78 万次编译

改成 `static const QRegularExpression`（只编译一次）后：

- `generateHtml` 从 **1756 ms → 84 ms（21 倍）**
- 大纲刷新从 **1221 ms → 203 ms（6 倍）**

**根因二：打开文件时 `updatePreview()` 被调用 4 次**

调用链：`newFile()` 一次 → `openFile()` 末尾一次 → `setText` 启动的 500 ms 防抖定时器再补一次
→ 事件循环里的再次触发。每次都是全量 `toPlainText` + `generateHtml` + `setHtml`，
大文档下纯属浪费。

修法：`updatePreview()` 加内容缓存，内容未变直接返回（先比长度，再比内容）。
注意**主题切换时必须清缓存**——主题变了生成的 HTML 也变了，但 Markdown 内容没变。

> 2026-10 更新：这一条后来又改了两版。
> 先是「拷贝全文比对」本身就很贵（5 MB 文档等于每次 10 MB 分配），
> 现在改成用 `textChanged` 驱动的**内容版本号**做键；
> 又因为预览区不再内嵌 `<style>`（样式走 `QTextDocument::setDefaultStyleSheet`），
> 换主题也不再需要清缓存了。详见 9.9。

**根因三（未完全解决）：`QTextBrowser::setHtml()` 是当前最大瓶颈**

5 MB 时仍占 1585 ms。这是 Qt 富文本引擎全量解析 + 布局的固有成本，
除非改成虚拟滚动（只渲染视口内的块，Typora/CodeMirror 的做法），否则难以进一步压缩。

### 9.4 编辑器内核替换：QTextEdit → QPlainTextEdit

完成 9.3 的两项优化后，`QTextBrowser::setHtml` 成了最大单项，但外部测量显示的
「首帧绘制」还要多花 4 秒以上。根因在编辑器本身：

**`QTextEdit` 是富文本编辑器**。即使内容全是纯文本，它也走 `QTextDocument`
的富文本布局，而且为了算出滚动条范围必须**全量布局**——86 K 行的文档就是一次
几秒的完整排版。`QPlainTextEdit` 专为纯文本/大文档设计，按行分块布局，成本低得多。

**改造前先确认可行性**（这一步很关键，避免白做）：

```bash
grep -c "QTextEdit" src/*.cpp src/*.h        # 86 处
grep -n "editor->setHtml|toHtml|appendHtml"  # 0 处
```

编辑器上**没有任何富文本 API 调用**——本来就是纯文本用途，换内核不会丢功能。
实际只有 2 个 API 不兼容：

| 不兼容的 API | 处理 |
|---|---|
| `setText()` | 改 `setPlainText()` |
| `setAcceptRichText(false)` | 直接删掉（QPlainTextEdit 本身就是纯文本） |

其余 24 个方法（`textCursor` / `find` / `setTextCursor` / `document` /
`verticalScrollBar` / `cursorRect` / `setTabStopDistance` …）两者签名一致。

`QSyntaxHighlighter` 对两者通用，语法高亮不用改。

### 9.5 重构中踩到的坑

都属于「类型一换，布局/状态全变」这类隐性回归，编译能过、自检能过，
只有**截图对比**和**实际启动**才暴露：

| 坑 | 现象 | 根因与修法 |
|---|---|---|
| **编辑器被压成窄条** | 源码区只显示 3~4 个字就换行，整块挤在左边 | `QPlainTextEdit` 的 `sizeHint` 比 `QTextEdit` 小得多（它按内容行数估，不像 QTextEdit 给固定富文本尺寸）。`QSplitter` 首次布局按 sizeHint 分配 → 给编辑器 `setMinimumWidth(360)` + `setSizes(<<1<<1)` |
| **侧边栏被挤窄** | 大纲标题全被省略号截断 | 中央区要求更多宽度，`QDockWidget` 被压缩到 `minimumSizeHint`。给 `m_sidebarTabs` 设 `minimumWidth(200)`，并用 `resizeDocks(..., {380}, Qt::Horizontal)` 定初始宽度 |
| **打开文件后多一个「未命名」标签** | 标签栏变成「未命名 \| 文件名」两个 | 构造函数建了初始空标签，`openFile` 里又 `newFile()`。加复用判断——但**判断里不能看 `modified`**：主题切换、高亮刷新都会触发 `textChanged` 把它置真（实测 `--dark` 启动就会），只看「无路径 + 无内容」才稳 |
| **换行模式** | 长行不换行、出现横向滚动条 | `QPlainTextEdit` 的 `lineWrapMode` 默认值在不同 Qt 版本有差异，显式设 `WidgetWidth` |

**顺带的收益**：打字机模式的居中改用 `QPlainTextEdit` 内置的 `centerCursor()`。
原先手算「光标矩形中心 − 视口中心」再改滚动条，在行高不一致的文档里
（代码块、标题行高不同）会积累偏差；内置实现按块布局精确计算。

### 9.6 现在的瓶颈分布

5 MB 文档的阶段耗时（内部 3210 ms）：

| 阶段 | 耗时 | 占比 |
|---|---|---|
| 读文件 + UTF-8 解码 | 231 ms | 7% |
| 编辑器载入 + 语法高亮 | 556 ms | 17% |
| 大纲刷新 | 175 ms | 5% |
| 生成 HTML | 438 ms | 14% |
| **预览渲染（`QTextBrowser::setHtml`）** | **1504 ms** | **47%** |
| 收尾 | 304 ms | 9% |

**预览渲染现在是唯一的大头**，占近一半。它和编辑器是同一个问题：
`QTextBrowser` 也是 `QTextEdit` 的子类，同样要全量富文本布局。
但预览必须渲染 HTML，不能用 `QPlainTextEdit` 顶替。

两条可行路径（都未实施）：

1. **预览限流**：超大文档只渲染前 N 行 + 末尾提示，直接省掉 1.4 s
2. **分块渲染**：先渲染首屏若干块让窗口立刻可用，剩余部分空闲时分批补齐

### 9.7 剩余瓶颈与后续路径

5 MB 文档的阶段耗时（内部 3402 ms）：

| 阶段 | 耗时 | 说明 |
|---|---|---|
| 读文件 + UTF-8 解码 | 230 ms | 可接受 |
| 编辑器载入 + 语法高亮 | 589 ms | `QTextEdit::setPlainText` |
| 大纲刷新 | 203 ms | 已优化 |
| 生成 HTML | 464 ms | 已优化到接近下限 |
| **预览渲染** | **1585 ms** | `QTextBrowser` 全量布局，Qt 固有成本 |
| 收尾 | 279 ms | |

外部测量比内部多 4.4 s，这部分是**首帧绘制**：
`QTextEdit` 需要全量布局才能算出滚动条范围（Qt 的已知特性）。

进一步优化的两条路（都需较大改动，暂未实施）：

1. **预览限流**：超大文档只渲染前 N 行 + 末尾提示，可省掉 1585 ms
2. **编辑器换 `QPlainTextEdit`**：专为大文档优化、布局成本低得多，需重构所有 `currentEditor()` 调用点

### 9.8 顺带修掉的问题

- **窗口标题不含文件名**：改为 `文件名 - Markdown Editor`（与 Typora 的 `文件名 - Typora` 一致）
- **刚打开的文件显示「已修改」星号**：`setText` 会触发 `textChanged` 把 `modified` 置真，
  载入后必须立刻清掉

### 9.9 统一解析 + 内容缓存后的复测（2026-10）

这一轮做了两件事：**把预览区并入唯一一套 Markdown 解析**，
以及**给预览/大纲加内容版本缓存**。用 `git worktree` 拉出改动前的基线
单独构建，做同机同文档的 A/B 对照（`--bench-open` 内部打点，单位 ms）：

| 阶段 | 5 MB 基线 | 5 MB 现在 | 1 MB 基线 | 1 MB 现在 |
|---|---|---|---|---|
| `read_file` | 225 | 234 | 2.4 | 3.8 |
| `set_editor_text` | 818 | 799 | 116 | 116 |
| `outline` | 1041 | 870 | 155 | 131 |
| `preview_toPlainText` | 1048 | —（省掉） | 156 | —（省掉） |
| `preview_generateHtml` | 1616 | 1009 | 255 | 163 |
| `preview_setHtml` | 3343 | 2391 | 402 | 300 |
| **`open_total`** | **3367** | **2415（−28%）** | **407** | **306（−25%）** |
| **`wall_open_ms`** | **3667** | **2519（−31%）** | **465** | **330（−29%）** |

三处关键改动：

1. **判据从「拷贝全文比对」换成「内容版本号」**
   早期判断内容有没有变，办法是把全文 `toPlainText()` 出来再比对 ——
   5 MB 文档等于每次 10 MB 分配，比重渲染本身还贵。
   现在用 `textChanged` 驱动的版本号做键。
   （文档自己的 `revision()` 不能单独当判据：`clear()` 会把它重置回 -1，
   两次 `setPlainText` 之后可能撞回同一个值。）

2. **预览与大纲共用一份全文拷贝**，不再各拷一次。

3. **拆分整页 HTML 与 body 片段**
   预览区只渲染 body，不再每帧附带整份 `<style>` 与 `<script>`
   —— `QTextBrowser` 两者都不认，那几 KB 纯属白解析。
   换主题也因此不必重渲染预览了（样式本来就走
   `QTextDocument::setDefaultStyleSheet`）。

剩下的瓶颈依然是 `QTextBrowser::setHtml()`（5 MB 约 1.4 s），
即 Qt 富文本引擎的全量解析 + 布局，属固有成本，
要进一步压缩只能改成虚拟滚动。

### 9.10 复现方式

```powershell
# 生成测试文档（200KB / 1MB / 5MB）
python tools/gen_bench_docs.py benchdata

# 本项目的分阶段耗时（内部打点）
dist/MarkdownEditor.exe --bench-open report.txt benchdata/bench-1m.md

# 与其它编辑器的外部对比测量（两个程序用同一标准）
python tools/bench_open_external.py dist/MarkdownEditor.exe benchdata/bench-1m.md --rounds 3
python tools/bench_open_external.py "<Typora 安装目录>\Typora.exe" benchdata/bench-1m.md `
    --rounds 3 --kill Typora.exe --args "--disable-gpu|--disable-software-rasterizer|--no-sandbox"
```

**测量陷阱**：如果从 IDE/终端启动 Electron 应用，务必先清掉
`ELECTRON_RUN_AS_NODE` 与 `NODE_OPTIONS` 环境变量 ——
否则 Electron 会退化成纯 Node 模式，把 `.md` 当 JS 脚本执行并报 `SyntaxError`，
看起来像「Typora 安装损坏」。

## 十、应用图标

原先用的是界面图标集里那个 24×24 的线性轮廓 `app.svg`（`stroke="currentColor"`，
本来给菜单和工具栏用的），拿来当应用图标太单薄；而且 **exe 本身没有嵌图标** ——
资源管理器、任务栏、Alt+Tab 里显示的都是系统默认图标。

### 10.1 设计

符号选了 **M↓** —— Markdown 的通行标识，识别度最高。配色用蓝色渐变（技术工具的常见取向），
圆角矩形底版，白色前景。

设计上最容易踩的坑是**小尺寸**：完整版 `M↓` 在 16/24/32 像素下，箭头和 M 会糊成一团白点。
所以做了两份 SVG：

| 文件 | 用途 | 内容 |
|---|---|---|
| `resources/icons/app-icon.svg` | 48 / 64 / 128 / 256 | 完整 `M↓`，带渐变高光 |
| `resources/icons/app-icon-small.svg` | 16 / 20 / 24 / 32 | 只有粗体 `M`，去掉箭头 |

两者视觉语言一致（同色底、同字重），只是小尺寸做了减法。
实测 16px 下 `M` 依然清晰可辨。

### 10.2 生成 ICO

Windows 的 exe 图标需要 **16/24/32/48/64/128/256 多档**，否则任务栏和大图标视图会模糊。
`QImage::save("x.ico")` 只能写单一尺寸，所以自己拼 ICO：

- ICO 结构很简单：`ICONDIR`(6字节) + `ICONDIRENTRY`×N(16字节) + 各图数据
- Vista 以后允许每个条目直接嵌 PNG，不用转 BMP
- `width/height` 字段只有 1 字节，**0 表示 256**

实现放在程序里（`--export-icon`），因为渲染 SVG 需要 `QSvgRenderer`：

```powershell
dist/MarkdownEditor.exe --export-icon resources/app.ico
```

产物 7 档、30 KB，全部为 PNG 嵌入。

### 10.3 嵌入 exe

`app.rc` 只有一行（`IDI_ICON1 ICON "resources/app.ico"`），CMake 里按需挂上：

```cmake
if(WIN32)
    if(EXISTS resources/app.ico)
        enable_language(RC)
        target_sources(${PROJECT_NAME} PRIVATE app.rc)
    else()
        message(WARNING "未找到 app.ico，exe 将使用系统默认图标")
    endif()
endif()
```

**有个先有鸡还是先有蛋的问题**：生成 ICO 需要 exe，编译 exe 需要 ICO。
处理办法是 `app.ico` 作为产物提交到仓库，构建时直接用；
`build.ps1` 里加了一步增量检查 —— 如果 SVG 比 ICO 新就重新生成，
首次构建（还没有 exe）则跳过并提示，第二次构建自动补上。

### 10.4 窗口图标

`QIcon("xx.svg")` 只会拿一份图去缩放，小尺寸同样会糊。
所以用 `addFile` 按尺寸分别绑定（`src/appicon.h` 里的 `makeAppIcon()`）：

```cpp
icon.addFile(":/icons/app-icon-small.svg", QSize(16, 16));   // 小尺寸用简化版
icon.addFile(":/icons/app-icon.svg",       QSize(48, 48));   // 大尺寸用完整版
```

### 10.5 验证

| 项 | 结果 |
|---|---|
| ICO 结构 | 7 档（16/24/32/48/64/128/256），全部 PNG 嵌入，30 KB |
| exe 图标 | `ExtractIconExW` 能取出图标组（`tools/check_exe_icon.py`） |
| 窗口标题栏 | 16 px 下 `M↓` 清晰可辨 |
| 各尺寸可辨性 | 16/24/32 只有 M；48 以上出现箭头（见 `shots/icon-sizes.png`） |

## 十一、验证结果

| 验证项 | 结果 |
|---|---|
| 编译 | GNU 13.1.0 + Qt 6.11.2，**0 error 0 warning** |
| 启动 | 进程稳定运行，窗口正常显示 |
| 浅色模式渲染 | 标题/列表/引用/表格/代码块/水平线全部正确，中文无乱码 |
| 深色模式渲染 | 编辑器 + 预览区同步换肤，高亮配色正确 |
| 语法高亮 | 标题、行内代码、链接、删除线、斜体均正确 |
| 工具栏图标 | 39 个 SVG 全部正常渲染 |
| **大纲面板** | 层级正确、缩进正确、随内容实时刷新 |
| **TOC 目录** | 渲染为可点击链接，点击跳转对应标题 |
| **任务列表** | ☑/☐ 字形正常，点击回写源码 |
| **主题系统** | 7 套主题配色全部正确，可持久化 |
| **智能标点** | 引号/破折号/省略号转换正确，英文缩写不误转 |
| **单独预览模式** | 隐藏编辑器与侧边栏、预览区居中留白、可完全还原（7 项断言） |
| **菜单栏结构** | 7 个菜单（文件/编辑/段落/格式/视图/主题/帮助），分组与顺序按 Typora 的公开规格对齐 |
| **快捷键对齐** | 73 个绑定，Typora 语义键全部落实，0 冲突 |
| **行内格式引擎** | 加粗/斜体/删除线/行内代码/高亮/上标/下标 的包裹与取消全部正确（14 项断言） |
| **块级编辑引擎** | 标题升降级、6 种块类型切换、表格行列、脚注/目录/分割线/YAML 全部正确（19 项断言） |
| **侧边栏三视图** | 文件树 / 大纲 / 文档树，`Ctrl+Shift+1/2/3` 切换 |
| **打字机模式** | `F10` 开启/关闭状态正确 |
| **导出 14 种格式** | HTML/PDF/docx/odt/epub/LaTeX/RTF/OPML/Wiki/RST/Textile/图像/Markdown/纯文本（44 + 21 项断言） |
| **Word docx 实机验证** | **Word 16.0 打开成功**：48 段落 / 1 表格 / 2 页，标题 1-3 被识别、outline 级别正确 |
| **ODT 实机验证** | **Word 16.0 打开成功**：64 段落 / 337 词 / 1 表格 |
| **EPUB 规范性** | mimetype 首位、manifest/spine 一致、XHTML 良构、nav 语义齐备 |
| **PDF 中文** | 5 处字体流嵌入、CID 编码、ToUnicode 映射齐备 |
| HTML 导出 | 23 项结构断言全部通过 |
| Typora 功能 | 18 项断言全部通过 |
| 主题 | 7 项断言全部通过 |
| 预览模式 | 11 项断言全部通过 |
| Typora UI 规格 | 57 项断言全部通过 |
| PDF 导出 | 生成有效 PDF-1.4（含完整字体引用） |
| **大文档性能** | 200KB~1.5MB 与 Typora 持平；2MB+ Typora 拒绝打开，本项目可开 5MB |
| **QPlainTextEdit 重构** | 86 处类型替换、2 个不兼容 API；1MB 提速 20%、5MB 提速 13% |
| **性能优化** | `generateHtml` 提速 21 倍（正则改静态）、大纲刷新提速 6 倍、预览去重 |
| **应用图标** | M↓ 设计，7 档 ICO（16~256），已嵌入 exe；16px 下清晰可辨 |
| 快捷键 | 74 个绑定，0 冲突 |

**自检总计 275 项断言全部通过**：单元测试 94 项（DocModel 38 + BlockEditor 30 + InlineFormatter 10 + SmartPunctuation 16）+ 脚本自检 181 项（渲染 23 + Typora 功能 18 + 主题 7 + 预览模式 11 + Typora UI 57 + 导出 44 + 导出深度 21）。

---

## 十二、说明

- 本项目为独立实现：解析器、渲染器、14 种导出格式全部自研，
  不包含任何第三方编辑器的代码或资源。
- 界面与交互习惯参考 Typora，仅为产品层面的对照，未做任何形式的逆向工程。
- 若需 MSVC 编译：把 `build.ps1` 中的 `CMAKE_CXX_COMPILER` 指向 VS 的 `cl.exe`，
  并把 `CMAKE_PREFIX_PATH` 改为 MSVC 版 Qt 路径。
  注意：MSVC 版 Qt 与 MinGW 版 Qt 不能混用，需分别安装。
- 许可证：尚未添加。如需开源请先补一个 `LICENSE`（MIT / Apache-2.0 均可）。
