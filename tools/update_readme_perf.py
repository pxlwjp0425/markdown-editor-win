#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""在 README 加「九、性能：大文档打开速度」章节。"""
import io

p = 'README.md'
s = io.open(p, encoding='utf-8').read()

SECTION = '''## 九、性能：大文档打开速度

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

| 文档 | 字符 / 行 | 本项目 标题就绪 | 本项目 渲染完成 | Typora 标题就绪 | Typora 渲染完成 |
|---|---|---|---|---|---|
| 200 KB | 94 K / 3.5 K | **1.12 s** | **2.00 s** | 1.66 s | 2.42 s |
| 1 MB | 471 K / 17 K | **1.76 s** | 3.24 s | 2.34 s | **3.10 s** |
| 1.5 MB | 708 K / 26 K | **1.68 s** | **3.20 s** | 2.62 s | 3.38 s |
| 2 MB | 940 K / 34 K | 1.86 s | 3.62 s | **拒绝打开** | — |
| 5 MB | 2.36 M / 86 K | 4.07 s | 7.80 s | **拒绝打开** | — |

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

**根因三（未完全解决）：`QTextBrowser::setHtml()` 是当前最大瓶颈**

5 MB 时仍占 1585 ms。这是 Qt 富文本引擎全量解析 + 布局的固有成本，
除非改成虚拟滚动（只渲染视口内的块，Typora/CodeMirror 的做法），否则难以进一步压缩。

### 9.4 剩余瓶颈与后续路径

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

### 9.5 顺带修掉的问题

- **窗口标题不含文件名**：改为 `文件名 - Markdown Editor`（与 Typora 的 `文件名 - Typora` 一致）
- **刚打开的文件显示「已修改」星号**：`setText` 会触发 `textChanged` 把 `modified` 置真，
  载入后必须立刻清掉

### 9.6 复现方式

```powershell
# 生成测试文档（200KB / 1MB / 5MB）
python tools/gen_bench_docs.py benchdata

# 本项目的分阶段耗时（内部打点）
dist/MarkdownEditor.exe --bench-open report.txt benchdata/bench-1m.md

# 与 Typora 的外部对比测量（两个程序用同一标准）
python tools/bench_open_external.py dist/MarkdownEditor.exe benchdata/bench-1m.md --rounds 3
python tools/bench_open_external.py "<Typora 安装目录>\\Typora.exe" benchdata/bench-1m.md `
    --rounds 3 --kill Typora.exe --args "--disable-gpu|--disable-software-rasterizer|--no-sandbox"
```

**测量陷阱**：如果从 IDE/终端启动 Electron 应用，务必先清掉
`ELECTRON_RUN_AS_NODE` 与 `NODE_OPTIONS` 环境变量 ——
否则 Electron 会退化成纯 Node 模式，把 `.md` 当 JS 脚本执行并报 `SyntaxError`，
看起来像「Typora 安装损坏」。

'''

marker = '## 九、验证结果'
assert marker in s
s = s.replace(marker, SECTION + '## 十、验证结果', 1)
s = s.replace('## 十、说明', '## 十一、说明', 1)

io.open(p, 'w', encoding='utf-8', newline='\n').write(s)
print('README 已加「九、性能」章节')
