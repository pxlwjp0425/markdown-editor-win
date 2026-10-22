#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""统一自检 key 为纯 ASCII（块级/段落/表格/打字机部分）。"""
import io
import re

p = 'src/mainwindow.cpp'
s = io.open(p, encoding='utf-8').read()

# key -> ASCII 名
RENAMES = {
    'blk_提升标题': 'blk_inc_heading',
    'blk_降低标题': 'blk_dec_heading',
    'blk_一级转段落': 'blk_h1_to_para',
    'blk_引用': 'blk_quote',
    'blk_无序列表': 'blk_ul',
    'blk_有序列表': 'blk_ol',
    'blk_任务列表': 'blk_task',
    'blk_任务列表完成': 'blk_task_done',
    'blk_代码块': 'blk_code',
    'blk_公式块': 'blk_math',
    'para_内容目录': 'para_toc',
    'para_水平分割线': 'para_hr',
    'para_YAML': 'para_yaml',
    'para_脚注引用': 'para_fn_ref',
    'para_脚注定义': 'para_fn_def',
    'tbl_插入行': 'tbl_insert_row',
    'tbl_插入列': 'tbl_insert_col',
    'tw_开启': 'tw_on',
    'tw_关闭': 'tw_off',
    # 菜单 / 页签（这些是拼接出来的，单独处理）
    '菜单_': 'menu_',
    '页签_': 'tab_',
}

count = 0
for cn, en in RENAMES.items():
    pat = re.compile(r'QStringLiteral\("' + re.escape(cn) + r'"\)')
    new_s, n = pat.subn('QStringLiteral("%s")' % en, s)
    if n:
        s = new_s
        count += n

# 菜单/页签是 QStringLiteral("菜单_") + n 的形式
s = s.replace('add(QStringLiteral("菜单_") + n,', 'add(QStringLiteral("menu_") + n,')
s = s.replace('add(QStringLiteral("页签_") + t,', 'add(QStringLiteral("tab_") + t,')

io.open(p, 'w', encoding='utf-8', newline='\n').write(s)
print('  %d 个 key 统一为 ASCII' % count)

# 校验：自检输出里不应再有非 ASCII 的 key
leftover = re.findall(r'QStringLiteral\("([^"]*[\u4e00-\u9fa5][^"]*)"\)', s)
if leftover:
    # 只关心用于 add() 的 key
    print('  仍有中文 key（可能用在其它地方）:', set(leftover))
