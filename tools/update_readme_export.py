#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""在 README 里加「七、多格式导出」章节并顺延后续编号。"""
import io

p = 'README.md'
s = io.open(p, encoding='utf-8').read()

SECTION = '''## 七、多格式导出

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
| RTF | `.rtf` | 从 IR 生成，中文走 `\\uN?` 转义 | 校验括号配平、字体表 |
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
DocBlock   — Heading / Paragraph / ListItem / Quote / CodeBlock / Table / Hr / MathBlock / RawHtml
```

`DocModel::parse()` 把 Markdown 解析成 `QList<DocBlock>`，然后 8 个格式生成器
（docx / EPUB / XHTML / LaTeX / RTF / OPML / MediaWiki / RST / Textile / Markdown）
全部从同一棵 IR 出发。好处：

- 各格式的语义一致（不会出现 docx 认得出标题、RTF 认不出）
- 行内解析与块级解析各自可单测
- 加新格式只需写一个 `toXxx()` 函数

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

'''

marker = '## 七、目录结构'
assert marker in s
s = s.replace(marker, SECTION + '## 八、目录结构', 1)
s = s.replace('## 八、验证结果', '## 九、验证结果', 1)
s = s.replace('## 九、说明', '## 十、说明', 1)

io.open(p, 'w', encoding='utf-8', newline='\n').write(s)
print('README 已加「七、多格式导出」')
