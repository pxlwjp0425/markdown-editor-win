#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
功能自检：验证 Markdown 渲染逻辑（generateHtml）的正确性。

不依赖 GUI，直接对渲染结果做断言。
通过在程序中加一个 --self-test 模式调用。
"""
import os
import subprocess
import sys

EXE = sys.argv[1]
OUT = sys.argv[2]
os.makedirs(OUT, exist_ok=True)

SAMPLE = os.path.join(OUT, 'selftest.md')
with open(SAMPLE, 'w', encoding='utf-8') as f:
    f.write("""# 标题一

段落 **粗体** *斜体* ~~删除线~~ `代码` [链接](https://a.com)。

## 标题二

- 项目甲
- 项目乙

1. 第一
2. 第二

> 引用内容

| A | B |
|---|---|
| 1 | 2 |

```js
var x = 1;
```

---

结尾
""")

# 程序以 --export-html 模式运行时，把渲染结果写到指定文件
html_out = os.path.join(OUT, 'selftest.html')
r = subprocess.run([EXE, SAMPLE, '--export-html', html_out],
                   cwd=os.path.dirname(EXE),
                   capture_output=True, text=True, timeout=30,
                   encoding='utf-8', errors='replace')

if not os.path.exists(html_out):
    print('FAIL: 未生成 HTML 文件')
    print('stdout:', r.stdout[-500:])
    print('stderr:', r.stderr[-500:])
    sys.exit(1)

with open(html_out, encoding='utf-8') as f:
    html = f.read()

print('HTML 长度:', len(html))

CHECKS = [
    # 标题现在带 id 锚点（Typora 风格，用于 [TOC] 与大纲跳转）
    ('<h1 id="标题一">标题一</h1>', '一级标题'),
    ('<h2 id="标题二">标题二</h2>', '二级标题'),
    ('<strong>粗体</strong>',  '粗体'),
    ('<em>斜体</em>',          '斜体'),
    ('<del>删除线</del>',      '删除线'),
    ('<code>代码</code>',      '行内代码'),
    ('<a href="https://a.com">链接</a>', '链接'),
    ('<ul>',                  '无序列表'),
    ('<ol>',                  '有序列表'),
    ('<li>项目甲</li>',        '列表项'),
    ('<blockquote>',          '引用块'),
    ('<table>',               '表格'),
    ('<th>A</th>',             '表头'),
    ('<pre><code class="language-js">', '代码块+语言标记'),
    ('<hr>',                  '水平线'),
    ('<p>结尾</p>',            '段落'),
]

fails = []
for needle, label in CHECKS:
    ok = needle in html
    print('  %-6s %-16s %s' % ('PASS' if ok else 'FAIL', label, needle[:44]))
    if not ok:
        fails.append(label)

# ---------------------------------------------------------------------------
# 预览 / 导出一致性回归
#
# 早期预览区有一套独立的正则状态机，与导出用的 DocModel::parse 对同一份
# 文档给出不同结果。下面每一项都是当时真实存在的差异。
# ---------------------------------------------------------------------------
DIVERGENCE_SAMPLE = os.path.join(OUT, 'divergence.md')
with open(DIVERGENCE_SAMPLE, 'w', encoding='utf-8') as f:
    f.write("""# 顶层

- 父项
  - 子项

| 左 | 中 | 右 |
|:--|:-:|--:|
| 1 | 2 | 3 |

行内代码 `**不该变粗**` 结束。

~~~text
# 这不是标题
~~~

## 闭合标题 ##

***粗斜体***

$$
E = mc^2
$$
""")

div_out = os.path.join(OUT, 'divergence.html')
subprocess.run([EXE, DIVERGENCE_SAMPLE, '--export-html', div_out],
               cwd=os.path.dirname(EXE),
               capture_output=True, text=True, timeout=30,
               encoding='utf-8', errors='replace')

with open(div_out, encoding='utf-8') as f:
    div = f.read()

DIVERGENCE_CHECKS = [
    # 嵌套列表：子列表必须在父项的 <li> 内部（HTML 语义要求）
    ('<ul><li>父项<ul><li>子项</li></ul></li></ul>', '嵌套列表结构'),
    # 表格：不应多出首尾两个空列（早期 split('|') 不剥首尾 |）
    ('<th align="left">左</th><th align="center">中</th><th align="right">右</th>',
     '表格对齐'),
    # 行内代码内部不再被当作行内标记解析
    ('<code>**不该变粗**</code>', '行内代码不二次解析'),
    # ~~~ 围栏代码块（早期只认 ```）
    ('<pre><code class="language-text"># 这不是标题</code></pre>', '~~~ 围栏代码块'),
    # 标题尾部的闭合 ##（早期预扫描剥了、主循环没剥，文本与锚点不一致）
    ('<h2 id="闭合标题">闭合标题</h2>', '标题尾部 #'),
    # *** 粗斜体（早期会多出一个孤立的 *）
    ('<strong><em>粗斜体</em></strong>', '粗斜体'),
    # 公式块（早期预览完全不识别）
    ('<pre class="math">', '公式块'),
]

print('')
for needle, label in DIVERGENCE_CHECKS:
    ok = needle in div
    print('  %-6s %-16s %s' % ('PASS' if ok else 'FAIL', label, needle[:44]))
    if not ok:
        fails.append(label)

# 不应出现的：段落被错误包裹在标签内
if '<p><h1>' in html:
    fails.append('标题被错误包进 <p>')
    print('  FAIL  标题被错误包进 <p>')

if '<p><ul>' in html:
    fails.append('列表被错误包进 <p>')
    print('  FAIL  列表被错误包进 <p>')

total = len(CHECKS) + len(DIVERGENCE_CHECKS)
if len(fails) == 0:
    print('\n全部 %d 项检查通过' % total)
    sys.exit(0)
else:
    print('\n%d 项失败: %s' % (len(fails), ', '.join(fails)))
    sys.exit(1)
