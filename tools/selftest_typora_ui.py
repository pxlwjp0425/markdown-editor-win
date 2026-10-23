#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Typora 风格功能自检（第四阶段）。

调程序内置的 --self-test-typora-ui，验证：
  - 菜单结构（7 个菜单 + 关键菜单项存在）
  - 快捷键对齐（Typora 语义的关键键）
  - 行内格式引擎（加粗/斜体/删除线/高亮/上下标的包裹与取消）
  - 块级编辑引擎（标题升降级、块类型切换、表格行列、脚注、目录）
  - 侧边栏三页签
  - 打字机模式
"""
import os
import subprocess
import sys

EXE = sys.argv[1]
OUT = sys.argv[2]
os.makedirs(OUT, exist_ok=True)

r = subprocess.run([EXE, '--self-test-typora-ui'],
                   cwd=os.path.dirname(EXE),
                   capture_output=True, timeout=60)

# stdout 可能是 UTF-8，也可能是本地编码，逐个尝试
raw = r.stdout
for enc in ('utf-8', 'gbk', 'latin-1'):
    try:
        out = raw.decode(enc)
        break
    except UnicodeDecodeError:
        continue
else:
    out = raw.decode('utf-8', errors='replace')
out = out.strip()
if not out:
    print('FAIL: 无输出 (exit=%d)' % r.returncode)
    print(r.stderr[:500])
    sys.exit(1)

print(out)

# 逐行解析 KEY=VALUE
kv = {}
for line in out.splitlines():
    if '=' in line:
        k, v = line.split('=', 1)
        kv[k.strip()] = v.strip()

# (键, 期望值)
CHECKS = [
    # 菜单结构
    ('menu_count', '7'),
    ('menu_file', '1'),
    ('menu_edit', '1'),
    ('menu_para', '1'),
    ('menu_format', '1'),
    ('menu_view', '1'),
    ('menu_theme', '1'),
    ('menu_help', '1'),
    # 侧边栏
    ('sidebar_tabs', '2'),
    ('tab_filetree', '1'),
    ('tab_doctree', '1'),
    ('outline_dock', '1'),
    # 快捷键对齐（Typora 语义，字符串为 Qt 的 toString() 形式）
    ('sc_h1', 'Ctrl+1'),
    ('sc_paragraph', 'Ctrl+0'),
    ('sc_inc_heading', 'Ctrl+['),
    ('sc_dec_heading', 'Ctrl+]'),
    ('sc_source_mode', 'Ctrl+/'),
    ('sc_focus_mode', 'F9'),
    ('sc_typewriter', 'F10'),
    ('sc_sidebar', 'F11'),
    ('sc_outline', 'Ctrl+Shift+1'),
    ('sc_doctree', 'Ctrl+Shift+2'),
    ('sc_filetree', 'Ctrl+Shift+3'),
    ('sc_actual_size', 'Ctrl+Shift+9'),
    ('sc_close_tab', 'Ctrl+W'),
    # 源码模式：两个勾选项必须同步，且高亮必须真的被关掉
    ('src_mode_menu_checked', '1'),
    ('src_mode_toolbar_checked', '1'),
    ('src_mode_highlight_off', '1'),
    ('src_mode_formats_present_before', '1'),
    ('src_mode_formats_cleared', '1'),
    ('src_mode_restored', '1'),
    ('src_mode_unchecked_after', '1'),
    # 行内格式
    ('fmt_bold_wrap', '**文本**'),
    ('fmt_bold_unwrap', '文本'),
    ('fmt_italic_wrap', '*文本*'),
    ('fmt_strike_wrap', '~~文本~~'),
    ('fmt_highlight_wrap', '==文本=='),
    ('fmt_sup_wrap', '^文本^'),
    ('fmt_sub_wrap', '~文本~'),
    # 块级编辑
    ('blk_inc_heading', '## 标题'),
    ('blk_dec_heading', '# 标题'),
    ('blk_h1_to_para', '标题'),
    ('blk_quote', '> 文字'),
    ('blk_ul', '- 文字'),
    ('blk_ol', '1. 文字'),
    ('blk_task', '- [ ] 文字'),
    ('blk_code', '```'),
    ('blk_math', '$$'),
    # 段落插入
    ('para_toc', '[TOC]'),
    ('para_hr', '---'),
    ('para_fn_ref', '[^fn1]'),
    ('para_fn_def', '[^fn1]: 脚注内容'),
    ('para_yaml', 'title:'),
    # 表格
    ('tbl_insert_row', '| 新行 |'),
    ('tbl_insert_col', '| 新列 |'),
    # 打字机
    ('tw_on', '1'),
    ('tw_off', '0'),
]

fails = []
for key, expect in CHECKS:
    got = kv.get(key)
    ok = got == expect
    print('  %-6s %-18s = %s' % ('PASS' if ok else 'FAIL', key, got))
    if not ok:
        fails.append('%s(期望%s,实际%s)' % (key, expect, got))

if fails:
    print('\n%d 项失败:' % len(fails))
    for f in fails:
        print('   -', f)
    sys.exit(1)

print('\n全部 %d 项检查通过' % len(CHECKS))
sys.exit(0)
