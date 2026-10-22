#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""更新 README 的性能章节：加入 QTextEdit → QPlainTextEdit 重构的结果。"""
import io
import re

p = 'README.md'
s = io.open(p, encoding='utf-8').read()

# ---- 1) 更新结果表：加一列「换 QPlainTextEdit 后」 ----
old_table = '''| 文档 | 字符 / 行 | 本项目 标题就绪 | 本项目 渲染完成 | Typora 标题就绪 | Typora 渲染完成 |
|---|---|---|---|---|---|
| 200 KB | 94 K / 3.5 K | **1.12 s** | **2.00 s** | 1.66 s | 2.42 s |
| 1 MB | 471 K / 17 K | **1.76 s** | 3.24 s | 2.34 s | **3.10 s** |
| 1.5 MB | 708 K / 26 K | **1.68 s** | **3.20 s** | 2.62 s | 3.38 s |
| 2 MB | 940 K / 34 K | 1.86 s | 3.62 s | **拒绝打开** | — |
| 5 MB | 2.36 M / 86 K | 4.07 s | 7.80 s | **拒绝打开** | — |'''

new_table = '''| 文档 | 字符 / 行 | 本项目 | 换 QPlainTextEdit 后 | Typora |
|---|---|---|---|---|---|
| 200 KB | 94 K / 3.5 K | 2.00 s | **2.00 s** | 2.42 s |
| 1 MB | 471 K / 17 K | 3.24 s | **2.58 s**（−20%） | 3.10 s |
| 1.5 MB | 708 K / 26 K | 3.20 s | **2.89 s**（−10%） | 3.38 s |
| 2 MB | 940 K / 34 K | 3.62 s | **3.29 s**（−9%） | **拒绝打开** |
| 5 MB | 2.36 M / 86 K | 7.80 s | **6.75 s**（−13%） | **拒绝打开** |

（「本项目」列是 QTextEdit 时期的数据，「换 QPlainTextEdit 后」是当前版本。
两个「本项目」列都指渲染完成时间，测试方法同为外部统一标准。）'''

assert old_table in s
s = s.replace(old_table, new_table, 1)

# ---- 2) 在 9.4 之前插入新的 9.4 节，原 9.4/9.5/9.6 顺延 ----
NEW_SECTION = '''### 9.4 编辑器内核替换：QTextEdit → QPlainTextEdit

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

'''

marker = '### 9.4 剩余瓶颈与后续路径'
assert marker in s, '未找到原 9.4 节'
s = s.replace(marker, NEW_SECTION + '### 9.7 剩余瓶颈与后续路径', 1)

# 原 9.5 / 9.6 顺延
s = s.replace('### 9.5 顺带修掉的问题', '### 9.8 顺带修掉的问题', 1)
s = s.replace('### 9.6 复现方式', '### 9.9 复现方式', 1)

io.open(p, 'w', encoding='utf-8', newline='\n').write(s)
print('README 性能章节已更新')
