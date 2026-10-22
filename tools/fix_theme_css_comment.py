#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
修复 theme.cpp 中 CSS 注释导致的 C++ 块注释提前终止问题。

问题：previewCss() 里的 CSS 字符串含有 "/* ... */" 形式的注释，
      其中 "*/" 会提前结束外层 C++ 块注释，导致后续代码被误解析。
解法：把 CSS 注释里的 "*/" 改写为 "&#42;/" 之类的无害形式，
      或者干脆把 CSS 注释移到 C++ 行注释之外。
这里采用：把 CSS 内的注释改为不含 "*/" 的写法。
"""
import io
import sys

path = sys.argv[1]
with io.open(path, 'r', encoding='utf-8') as f:
    src = f.read()

# CSS 注释中含 "*/" 的两处，改为不含结束符的写法
before = src

src = src.replace(
    '"/* 目录 —— 对应 Typora 的 .md-toc */\\n"',
    '"// 目录 —— 对应 Typora 的 .md-toc\\n"'
)
src = src.replace(
    '"/* 数学公式 —— 对应 Typora 的 .md-math-block / .md-inline-math-container */\\n"',
    '"// 数学公式 —— 对应 Typora 的 .md-math-block / .md-inline-math-container\\n"'
)
src = src.replace(
    '"/* 脚注 */\\n"',
    '"// 脚注\\n"'
)
src = src.replace(
    '"/* YAML front matter */\\n"',
    '"// YAML front matter\\n"'
)

if src == before:
    print('  未找到需要替换的 CSS 注释（可能已修复）')
else:
    with io.open(path, 'w', encoding='utf-8', newline='\n') as f:
        f.write(src)
    print('  已修复 CSS 注释中的块注释终止符')

# 复查：整个文件不应再出现 CSS 形式的块注释
remaining = []
for i, line in enumerate(src.split('\n'), 1):
    if '"/*' in line:
        remaining.append((i, line.strip()))
if remaining:
    print('  仍存在 CSS 块注释:')
    for i, l in remaining:
        print('    %d: %s' % (i, l))
else:
    print('  OK: 无残留 CSS 块注释')
