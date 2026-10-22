#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
消除快捷键冲突。

取舍原则（对齐 Typora 的实际行为）：
  - Typora 在「段落」「格式」「视图」三个菜单里会复用同一个快捷键
    （如 Ctrl+Shift+C 既是「代码块」也是「复制为 Markdown」）。
    本项目是单窗口全局快捷键，必须保证唯一，
    因此按「编辑类 > 视图类 > 格式类」的优先级保留一方。
  - 与 Typora 语义直接对应的（段落/格式/视图菜单里的）优先保留。
"""
import io
import re
import sys

SRC = sys.argv[1]
with io.open(SRC, 'r', encoding='utf-8') as f:
    src = f.read()

# action -> 新快捷键（None 表示移除快捷键）
RESOLVE = {
    # Ctrl+0: 段落（Typora 段落菜单）胜出，缩放重置让位
    'm_zoomResetAction': None,
    # Ctrl+K: 删除线（Typora 格式菜单）胜出，链接让位到 Ctrl+Shift+K
    'm_linkAction': None,
    # Ctrl+P: 打印（Typora 文件菜单）胜出，预览切换让位
    'm_togglePreviewAction': None,
    # Ctrl+Shift+C: 代码块（Typora 段落菜单）胜出，复制 Markdown 让位
    'm_copyMarkdownAction': None,
    # Ctrl+Shift+D: 显示源代码（Typora 视图菜单）胜出，深色模式让位
    'm_darkModeAction': None,
    # Ctrl+Shift+I: 行内代码（Typora 段落/格式菜单）胜出，图像让位
    'm_imageAction': None,
    # Ctrl+Shift++: 上标胜出，放大让位到 Ctrl+=
    'm_zoomInAction': 'Qt::CTRL | Qt::Key_Equal',
    # Ctrl+Shift+-: 下标胜出，缩小让位到 Ctrl+-
    'm_zoomOutAction': 'Qt::CTRL | Qt::Key_Minus',
    # Ctrl+Shift+L: 无序列表（Typora 段落菜单）胜出，侧边栏让位到 F11
    'm_toggleSidebarAction': None,
    # Ctrl+Shift+M: 公式块（Typora 段落菜单）胜出，源码模式让位到 Ctrl+/
    'm_toggleSourceModeAction': None,
    # Ctrl+Shift+N: 新建窗口（Typora 文件菜单）胜出，大纲切换让位到 Ctrl+Shift+1
    'm_toggleOutlineAction': None,
    # F11: 侧边栏（Typora 视图菜单）胜出，全屏让位
    'm_fullscreenAction': None,
}

changed = 0
for name, expr in RESOLVE.items():
    pat = re.compile(r'^([ \t]*)' + re.escape(name) + r'->setShortcut\([^;]*\);\n',
                     re.MULTILINE)
    if expr is None:
        # 移除快捷键（保留 action 本身）
        new_src, n = pat.subn('', src)
    else:
        new_src, n = pat.subn(
            lambda m: m.group(1) + name + '->setShortcut(QKeySequence(' + expr + '));\n',
            src)
    if n:
        src = new_src
        changed += n
    else:
        print('  [跳过]', name)

print('冲突处理 %d 处' % changed)

with io.open(SRC, 'w', encoding='utf-8', newline='\n') as f:
    f.write(src)
