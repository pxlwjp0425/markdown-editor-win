#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
把 Typora UI 自检的 key 全部改为 ASCII，避免中文 key 在控制台编码上出问题。
同时修正期望值（Qt 的 shortcut().toString() 输出形式）。
"""
import io
import re

# ===========================================================================
# 1) mainwindow.cpp: key 改 ASCII
# ===========================================================================
p = 'src/mainwindow.cpp'
s = io.open(p, encoding='utf-8').read()

KEY_MAP = {
    '菜单数量': 'menu_count',
    '菜单_文件': 'menu_file',
    '菜单_编辑': 'menu_edit',
    '菜单_段落': 'menu_para',
    '菜单_格式': 'menu_format',
    '菜单_视图': 'menu_view',
    '菜单_主题': 'menu_theme',
    '菜单_帮助': 'menu_help',
    '侧边栏页签数': 'sidebar_tabs',
    '页签_文件树': 'tab_filetree',
    '大纲dock可见': 'outline_dock',
    '页签_文档树': 'tab_doctree',
    '快捷键_标题1': 'sc_h1',
    '快捷键_段落': 'sc_paragraph',
    '快捷键_提升标题': 'sc_inc_heading',
    '快捷键_降低标题': 'sc_dec_heading',
    '快捷键_源码模式': 'sc_source_mode',
    '快捷键_专注模式': 'sc_focus_mode',
    '快捷键_打字机模式': 'sc_typewriter',
    '快捷键_侧边栏': 'sc_sidebar',
    '快捷键_大纲': 'sc_outline',
    '快捷键_文档树': 'sc_doctree',
    '快捷键_文件树': 'sc_filetree',
    '快捷键_实际大小': 'sc_actual_size',
}

count = 0
for cn, en in KEY_MAP.items():
    pat = re.compile(r'QStringLiteral\("' + re.escape(cn) + r'"\)')
    new_s, n = pat.subn('QStringLiteral("%s")' % en, s)
    if n:
        s = new_s
        count += n
print('  mainwindow.cpp: %d 个 key 改 ASCII' % count)

# 格式/块级相关的 key 是拼接的（格式_xxx包裹），单独处理
for cn, en in [('格式_', 'fmt_'), ('块_', 'blk_'), ('段落_', 'para_'),
               ('表格_', 'tbl_'), ('打字机_', 'tw_')]:
    if cn in s:
        s = s.replace('QStringLiteral("%s' % cn, 'QStringLiteral("%s' % en)
        print('  前缀 %s -> %s' % (cn, en))

# 拼接形式：QString::fromLatin1(c.key) + QStringLiteral("包裹")
s = s.replace('QString::fromLatin1(c.key) + QStringLiteral("包裹")',
              'QString::fromLatin1(c.key) + QStringLiteral("_wrap")')
s = s.replace('QString::fromLatin1(c.key) + QStringLiteral("取消")',
              'QString::fromLatin1(c.key) + QStringLiteral("_unwrap")')

# 格式 case 表的 key 也是中文
s = s.replace('{"格式_加粗", "bold"}', '{"fmt_bold", "bold"}')
s = s.replace('{"格式_斜体", "italic"}', '{"fmt_italic", "italic"}')
s = s.replace('{"格式_删除线", "strikethrough"}', '{"fmt_strike", "strikethrough"}')
s = s.replace('{"格式_高亮", "highlight"}', '{"fmt_highlight", "highlight"}')
s = s.replace('{"格式_上标", "superscript"}', '{"fmt_sup", "superscript"}')
s = s.replace('{"格式_下标", "subscript"}', '{"fmt_sub", "subscript"}')

# 块级/段落/表格/打字机的 key
for cn, en in [
    ('块_提升标题', 'blk_inc_heading'), ('块_降低标题', 'blk_dec_heading'),
    ('块_一级转段落', 'blk_h1_to_para'), ('块_引用', 'blk_quote'),
    ('块_无序列表', 'blk_ul'), ('块_有序列表', 'blk_ol'),
    ('块_任务列表', 'blk_task'), ('块_任务列表完成', 'blk_task_done'),
    ('块_代码块', 'blk_code'), ('块_公式块', 'blk_math'),
    ('段落_内容目录', 'para_toc'), ('段落_水平分割线', 'para_hr'),
    ('段落_YAML', 'para_yaml'), ('段落_脚注引用', 'para_fn_ref'),
    ('段落_脚注定义', 'para_fn_def'),
    ('表格_插入行', 'tbl_insert_row'), ('表格_插入列', 'tbl_insert_col'),
    ('打字机_开启', 'tw_on'), ('打字机_关闭', 'tw_off'),
]:
    s = s.replace('QStringLiteral("%s")' % cn, 'QStringLiteral("%s")' % en)

io.open(p, 'w', encoding='utf-8', newline='\n').write(s)

# ===========================================================================
# 2) selftest_typora_ui.py: 同步 key 与期望值
# ===========================================================================
p2 = 'tools/selftest_typora_ui.py'
t = io.open(p2, encoding='utf-8').read()

NEW_CHECKS = '''CHECKS = [
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
    ('sidebar_tabs', '3'),
    ('tab_filetree', '1'),
    ('tab_outline', '1'),
    ('tab_doctree', '1'),
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
]'''

t = re.sub(r'CHECKS = \[.*?\n\]', NEW_CHECKS, t, flags=re.S)

# 值里含中文，仍会经过 stdout 编码 —— 解析时做 UTF-8 容错
t = t.replace('''r = subprocess.run([EXE, '--self-test-typora-ui'],
                   cwd=os.path.dirname(EXE),
                   capture_output=True, text=True, timeout=60)

out = r.stdout.strip()''', '''r = subprocess.run([EXE, '--self-test-typora-ui'],
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
out = out.strip()''')

io.open(p2, 'w', encoding='utf-8', newline='\n').write(t)
print('  selftest_typora_ui.py 已同步 ASCII key')
