#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Typora 风格功能自检：验证 TOC、锚点、任务列表、主题 CSS 的渲染结果。
"""
import os
import subprocess
import sys

EXE = sys.argv[1]
OUT = sys.argv[2]
os.makedirs(OUT, exist_ok=True)

SAMPLE = os.path.join(OUT, 'typora-selftest.md')
with open(SAMPLE, 'w', encoding='utf-8') as f:
    f.write("""[TOC]

# 一级标题

段落内容。

## 二级标题

### 三级标题

## 重复标题

### 重复标题

- [x] 已完成
- [ ] 未完成

- [ ] 带 **粗体** 的任务
""")

html_out = os.path.join(OUT, 'typora-selftest.html')
# 先删除旧文件，避免误读上一次的结果
if os.path.exists(html_out):
    os.remove(html_out)
r = subprocess.run([EXE, SAMPLE, '--export-html', html_out],
                   cwd=os.path.dirname(EXE),
                   capture_output=True, text=True, timeout=30,
                   encoding='utf-8', errors='replace')

if not os.path.exists(html_out):
    print('FAIL: 未生成 HTML')
    print('stdout:', r.stdout[-400:])
    print('stderr:', r.stderr[-400:])
    sys.exit(1)

with open(html_out, encoding='utf-8') as f:
    html = f.read()

print('HTML 长度:', len(html))

CHECKS = [
    # TOC 目录
    ('class="md-toc"',                    'TOC 容器'),
    ('class="md-toc-content"',            'TOC 内容区'),
    ('md-toc-h1',                          'TOC 一级标题样式类'),
    ('md-toc-h3',                          'TOC 三级标题样式类'),
    ('href="#一级标题"',                    'TOC 链接指向中文锚点'),
    ('md-toc-inner',                       'TOC 链接元素'),

    # 标题锚点
    ('<h1 id="一级标题">',                 '一级标题锚点'),
    ('<h2 id="二级标题">',                 '二级标题锚点'),
    ('<h3 id="三级标题">',                 '三级标题锚点'),

    # 锚点去重
    ('id="重复标题"',                      '首个同名标题锚点'),
    ('id="重复标题-1"',                    '重复标题锚点去重后缀'),

    # 任务列表
    ('md-task-list-item',                  '任务列表项'),
    ('md-task-box',                        '任务复选框元素'),
    ('href="#md-task:',                    '任务切换锚点'),
    ('☑',                                  '已勾选字形'),
    ('☐',                                  '未勾选字形'),

    # 主题 CSS
    ('.markdown-body',                     '正文容器'),
    ('--text-color',                       '主题变量存在'),
]

fails = []
for needle, label in CHECKS:
    ok = needle in html
    print('  %-6s %-22s %s' % ('PASS' if ok else 'FAIL', label, needle[:44]))
    if not ok:
        fails.append(label)

# 不应出现的问题
BAD = [
    ('<p><h1>',       '标题被包进段落'),
    ('<p><ul>',       '列表被包进段落'),
    ('<p><table>',    '表格被包进段落'),
    ('id=""',         '空锚点'),
]
for needle, label in BAD:
    if needle in html:
        fails.append(label)
        print('  FAIL   %-22s %s' % (label, needle))

if fails:
    print('\n%d 项失败: %s' % (len(fails), ', '.join(fails)))
    sys.exit(1)

print('\n全部 %d 项检查通过' % len(CHECKS))
sys.exit(0)
