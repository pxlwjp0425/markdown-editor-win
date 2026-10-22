#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
生成 Markdown Editor 的 SVG 图标集（Windows 版）。

为什么必须自带图标：
  原代码用 QIcon::fromTheme("document-new", fallback)，
  fromTheme 依赖 XDG/GTK 图标主题，Windows 上没有该机制，
  会静默退化成空图标 —— 工具栏变成一排空白按钮。
  这里生成内置 SVG 并打包进 qrc，保证跨平台一致。

图标风格：24x24 viewBox，currentColor 描边，随主题着色。
"""
import io
import os
import sys

OUT = sys.argv[1]
os.makedirs(OUT, exist_ok=True)

# 统一前缀/后缀，套一层 <g> 控制描边属性
HEAD = ('<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" '
        'width="24" height="24" fill="none" stroke="currentColor" '
        'stroke-width="1.7" stroke-linecap="round" stroke-linejoin="round">\n')
TAIL = '</svg>\n'


def svg(body):
    return HEAD + body + TAIL


ICONS = {
    # ---- 文件操作 ----
    "new": '<path d="M14 3H7a2 2 0 0 0-2 2v14a2 2 0 0 0 2 2h10a2 2 0 0 0 2-2V8z"/>\n'
           '<path d="M14 3v5h5"/>\n'
           '<path d="M12 12v6"/>\n<path d="M9 15h6"/>\n',

    "open": '<path d="M3 7a2 2 0 0 1 2-2h4l2 2.5h8a2 2 0 0 1 2 2V17a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2z"/>\n',

    "folder": '<path d="M3 7a2 2 0 0 1 2-2h4l2 2.5h8a2 2 0 0 1 2 2V17a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2z"/>\n',

    "save": '<path d="M5 3h11l3 3v13a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2V5a2 2 0 0 1 2-2z"/>\n'
            '<path d="M8 3v6h7V3"/>\n'
            '<rect x="8" y="13" width="8" height="8" rx="1"/>\n',

    # ---- 编辑操作 ----
    "undo": '<path d="M4 8h9a5 5 0 0 1 0 10H8"/>\n'
            '<path d="M4 8l4-4"/>\n<path d="M4 8l4 4"/>\n',

    "redo": '<path d="M20 8h-9a5 5 0 0 0 0 10h5"/>\n'
            '<path d="M20 8l-4-4"/>\n<path d="M20 8l-4 4"/>\n',

    "cut": '<circle cx="6" cy="18" r="2.5"/>\n<circle cx="18" cy="18" r="2.5"/>\n'
           '<path d="M8 16L18 4"/>\n<path d="M16 16L6 4"/>\n',

    "copy": '<rect x="9" y="9" width="12" height="12" rx="2"/>\n'
            '<path d="M5 15H4a1 1 0 0 1-1-1V4a1 1 0 0 1 1-1h10a1 1 0 0 1 1 1v1"/>\n',

    "paste": '<path d="M9 4H7a2 2 0 0 0-2 2v13a2 2 0 0 0 2 2h10a2 2 0 0 0 2-2V6a2 2 0 0 0-2-2h-2"/>\n'
             '<rect x="9" y="2" width="6" height="4" rx="1"/>\n'
             '<path d="M9 12h6"/>\n<path d="M9 16h4"/>\n',

    "find": '<circle cx="11" cy="11" r="7"/>\n'
            '<path d="M16.5 16.5L21 21"/>\n',

    "replace": '<circle cx="11" cy="11" r="7"/>\n'
               '<path d="M16.5 16.5L21 21"/>\n'
               '<path d="M8.5 11h5"/>\n',

    # ---- 格式：Markdown 语法直译 ----
    "bold": '<path d="M7 4h6.5a4 4 0 0 1 0 8H7z"/>\n'
            '<path d="M7 12h7.5a4 4 0 0 1 0 8H7z"/>\n',

    "italic": '<path d="M19 4h-7"/>\n<path d="M14 20H7"/>\n'
              '<path d="M15 4L9 20"/>\n',

    "strikethrough": '<path d="M5 12h14"/>\n'
                     '<path d="M16.5 7.5C15.8 5.8 14.2 4.8 12 4.8c-2.5 0-4.3 1.2-4.3 3 0 1.3.9 2.3 2.4 2.9"/>\n'
                     '<path d="M7.5 16c.8 1.9 2.4 3 4.7 3 2.6 0 4.4-1.3 4.4-3.2 0-1.2-.6-2.1-1.7-2.8"/>\n',

    "heading1": '<path d="M4 6v12"/>\n<path d="M12 6v12"/>\n'
                '<path d="M4 12h8"/>\n'
                '<path d="M17 10l2-2v10"/>\n',

    "heading2": '<path d="M4 6v12"/>\n<path d="M12 6v12"/>\n'
                '<path d="M4 12h8"/>\n'
                '<path d="M16 9.5a2.5 2.5 0 0 1 5 0c0 2-5 4.5-5 8.5h5"/>\n',

    "heading3": '<path d="M4 6v12"/>\n<path d="M12 6v12"/>\n'
                '<path d="M4 12h8"/>\n'
                '<path d="M16 9.5a2.5 2.5 0 0 1 4.6 1.3c0 1.4-1.2 2.2-2.4 2.2 1.3 0 2.5.8 2.5 2.3A2.6 2.6 0 0 1 16 17.7"/>\n',

    "code": '<path d="M9 6l-5 6 5 6"/>\n<path d="M15 6l5 6-5 6"/>\n',

    "codeblock": '<rect x="3" y="4" width="18" height="16" rx="2"/>\n'
                 '<path d="M8.5 9.5L6 12l2.5 2.5"/>\n'
                 '<path d="M15.5 9.5L18 12l-2.5 2.5"/>\n'
                 '<path d="M13 9l-2 6"/>\n',

    "link": '<path d="M10.5 13.5a4 4 0 0 0 5.7 0l3-3a4 4 0 0 0-5.7-5.7L11.8 6.5"/>\n'
            '<path d="M13.5 10.5a4 4 0 0 0-5.7 0l-3 3a4 4 0 0 0 5.7 5.7l1.7-1.7"/>\n',

    "image": '<rect x="3" y="4" width="18" height="16" rx="2"/>\n'
             '<circle cx="8.5" cy="9.5" r="1.5"/>\n'
             '<path d="M21 16l-5-5-6 6-3-3-4 4"/>\n',

    "table": '<rect x="3" y="4" width="18" height="16" rx="2"/>\n'
             '<path d="M3 9.5h18"/>\n<path d="M3 15h18"/>\n'
             '<path d="M9.5 4v16"/>\n<path d="M15 4v16"/>\n',

    "bulletlist": '<path d="M9 6h11"/>\n<path d="M9 12h11"/>\n<path d="M9 18h11"/>\n'
                  '<circle cx="4.5" cy="6" r="1.3"/>\n'
                  '<circle cx="4.5" cy="12" r="1.3"/>\n'
                  '<circle cx="4.5" cy="18" r="1.3"/>\n',

    "numberedlist": '<path d="M10 6h10"/>\n<path d="M10 12h10"/>\n<path d="M10 18h10"/>\n'
                    '<path d="M4 4.5h1V8"/>\n'
                    '<path d="M3.5 11h2.2L3.5 13.5h2.2"/>\n'
                    '<path d="M3.5 16.5h2.2v1.2H3.5"/>\n',

    "blockquote": '<path d="M6 6v12"/>\n'
                  '<path d="M10 8h10"/>\n<path d="M10 12h10"/>\n<path d="M10 16h6"/>\n',

    "horizontalrule": '<path d="M3 12h18"/>\n'
                      '<path d="M3 7h18"/>\n<path d="M3 17h18"/>\n',

    # ---- 导出 ----
    "exporthtml": '<path d="M14 3H7a2 2 0 0 0-2 2v14a2 2 0 0 0 2 2h10a2 2 0 0 0 2-2V8z"/>\n'
                  '<path d="M14 3v5h5"/>\n'
                  '<path d="M9 13l1.8 1.8L9 16.6"/>\n'
                  '<path d="M12.5 12v6"/>\n',

    "exportpdf": '<path d="M14 3H7a2 2 0 0 0-2 2v14a2 2 0 0 0 2 2h10a2 2 0 0 0 2-2V8z"/>\n'
                 '<path d="M14 3v5h5"/>\n'
                 '<path d="M8.5 17v-4h1.2a1.2 1.2 0 0 1 0 2.4H8.5"/>\n'
                 '<path d="M12.5 17v-4h1a2 2 0 0 1 0 4z"/>\n',

    # ---- 视图 ----
    "preview": '<path d="M2 12s3.6-7 10-7 10 7 10 7-3.6 7-10 7-10-7-10-7z"/>\n'
               '<circle cx="12" cy="12" r="3"/>\n',

    "sidebar": '<rect x="3" y="4" width="18" height="16" rx="2"/>\n'
               '<path d="M9.5 4v16"/>\n',

    "darkmode": '<path d="M20 14.5A8.5 8.5 0 0 1 9.5 4a8.5 8.5 0 1 0 10.5 10.5z"/>\n',

    "focusmode": '<path d="M4 9V5.5A1.5 1.5 0 0 1 5.5 4H9"/>\n'
                 '<path d="M15 4h3.5A1.5 1.5 0 0 1 20 5.5V9"/>\n'
                 '<path d="M20 15v3.5a1.5 1.5 0 0 1-1.5 1.5H15"/>\n'
                 '<path d="M9 20H5.5A1.5 1.5 0 0 1 4 18.5V15"/>\n',

    "fullscreen": '<path d="M4 9V5.5A1.5 1.5 0 0 1 5.5 4H9"/>\n'
                  '<path d="M15 4h3.5A1.5 1.5 0 0 1 20 5.5V9"/>\n'
                  '<path d="M20 15v3.5a1.5 1.5 0 0 1-1.5 1.5H15"/>\n'
                  '<path d="M9 20H5.5A1.5 1.5 0 0 1 4 18.5V15"/>\n',

    # ---- 缩放 ----
    "zoomin": '<circle cx="11" cy="11" r="7"/>\n'
              '<path d="M16.5 16.5L21 21"/>\n'
              '<path d="M8.5 11h5"/>\n<path d="M11 8.5v5"/>\n',

    "zoomout": '<circle cx="11" cy="11" r="7"/>\n'
               '<path d="M16.5 16.5L21 21"/>\n'
               '<path d="M8.5 11h5"/>\n',

    "zoomreset": '<circle cx="11" cy="11" r="7"/>\n'
                 '<path d="M16.5 16.5L21 21"/>\n'
                 '<path d="M8 11h6"/>\n',

    # ---- 其它 ----
    "app": '<rect x="2.5" y="4" width="19" height="16" rx="2.5"/>\n'
           '<path d="M6 9h12"/>\n<path d="M6 13h8"/>\n<path d="M6 16.5h5"/>\n',

    # ---- 大纲 / 源码模式（Typora 风格功能）----
    "outline": '<path d="M4 6h4"/>\n<path d="M12 6h8"/>\n'
               '<path d="M4 12h4"/>\n<path d="M12 12h8"/>\n'
               '<path d="M4 18h4"/>\n<path d="M12 18h8"/>\n',

    "codemode": '<path d="M9 6l-5 6 5 6"/>\n'
                '<path d="M15 6l5 6-5 6"/>\n',

    # 单独预览模式：矩形代表预览区，左侧被划掉表示隐藏编辑器
    "previewonly": '<rect x="3" y="4.5" width="18" height="15" rx="2"/>\n'
                   '<path d="M7.5 4.5v15"/>\n'
                   '<path d="M4.5 4.5L8 8"/>\n'
                   '<path d="M8 4.5L4.5 8"/>\n',
}

count = 0
for name, body in ICONS.items():
    path = os.path.join(OUT, name + ".svg")
    data = svg(body)
    # 内容相同就不重写：避免无谓的 mtime 变动触发 RCC 重跑，
    # 也避免 Ninja 并行构建时 qrc_icons.cpp 的依赖扫描竞争
    if os.path.exists(path):
        with io.open(path, 'r', encoding='utf-8') as f:
            if f.read() == data:
                count += 1
                continue
    with io.open(path, 'w', encoding='utf-8', newline='\n') as f:
        f.write(data)
    count += 1

# 生成 qrc（路径相对 qrc 所在目录，即 resources/）
# 手写的彩色应用图标（256 viewBox，多色渐变），内容不由本脚本生成，
# 但要一并打进 qrc —— 否则 QIcon(":/icons/app-icon.svg") 取不到。
EXTRA_ICONS = ['app-icon', 'app-icon-small']

qrc_lines = ['<RCC>', '    <qresource prefix="/icons">']
for name in sorted(list(ICONS.keys()) + EXTRA_ICONS):
    qrc_lines.append('        <file alias="%s.svg">icons/%s.svg</file>' % (name, name))
qrc_lines += ['    </qresource>', '</RCC>', '']
qrc_data = '\n'.join(qrc_lines)

qrc_path = os.path.join(OUT, '..', 'icons.qrc')
if os.path.exists(qrc_path):
    with io.open(qrc_path, 'r', encoding='utf-8') as f:
        if f.read() == qrc_data:
            print('generated %d icons (unchanged) + icons.qrc' % count)
            raise SystemExit(0)

with io.open(qrc_path, 'w', encoding='utf-8', newline='\n') as f:
    f.write(qrc_data)

print('generated %d icons + icons.qrc' % count)
