#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
给因冲突而被移除快捷键的 action 补上替代键（不与 Typora 对齐键冲突）。

这些键在 Typora 里本来就没有全局快捷键（只在特定菜单上下文里），
这里给它们安排 F 键 / Alt 组合，保证功能仍可键盘触达。
"""
import io
import re

p = 'src/mainwindow.cpp'
s = io.open(p, encoding='utf-8').read()

# action -> 替代快捷键表达式
ALT = {
    'm_linkAction':         'Qt::ALT | Qt::Key_L',      # Alt+L 插入链接
    'm_imageAction':        'Qt::ALT | Qt::Key_I',      # Alt+I 插入图片
    'm_togglePreviewAction': 'Qt::Key_F4',              # F4 显示/隐藏预览
    'm_darkModeAction':     'Qt::ALT | Qt::Key_D',      # Alt+D 深色模式
    'm_zoomResetAction':    'Qt::CTRL | Qt::SHIFT | Qt::Key_0',  # 实际大小
    'm_copyMarkdownAction': 'Qt::ALT | Qt::Key_M',      # Alt+M 复制 Markdown
    'm_toggleSidebarAction': 'Qt::ALT | Qt::Key_B',     # Alt+B 侧边栏
    'm_toggleSourceModeAction': 'Qt::ALT | Qt::Key_S',  # Alt+S 源码模式
    'm_toggleOutlineAction': 'Qt::ALT | Qt::Key_O',     # Alt+O 大纲
    'm_fullscreenAction':   'Qt::Key_F5',              # F5 全屏
}

count = 0
for name, expr in ALT.items():
    # 在 action 创建语句之后插入 setShortcut
    pat = re.compile(r'^([ \t]*)(' + re.escape(name) + r' = new QAction\([^;]*;\n)',
                     re.MULTILINE)
    insert = (lambda nm, ex: (lambda m: m.group(0) + m.group(1) + nm +
                              '->setShortcut(QKeySequence(' + ex + '));\n'))(name, expr)
    new_src, n = pat.subn(insert, s)
    if n:
        s = new_src
        count += n
    else:
        print('  [跳过]', name)

print('补替代快捷键 %d 处' % count)

io.open(p, 'w', encoding='utf-8', newline='\n').write(s)
