#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
把图标引用统一改为内置 SVG 资源。

背景：
  QIcon::fromTheme(name, fallback) 依赖 XDG/GTK/freedesktop 图标主题。
  Windows 没有该机制，fromTheme 永远走不到 fallback 分支，
  结果是工具栏按钮全部变成「无图标」的可点击空白方块。
  改为直接 QIcon(":/icons/xxx.svg")，跨平台表现一致。
"""
import io
import re
import sys

SRC = sys.argv[1]

with io.open(SRC, 'r', encoding='utf-8') as f:
    src = f.read()

# 1) QIcon::fromTheme("theme-name", QIcon(":/icons/xxx.png"))  ->  QIcon(":/icons/xxx.svg")
pattern = re.compile(
    r'QIcon::fromTheme\(\s*"[^"]*"\s*,\s*QIcon\(":/icons/([a-zA-Z0-9_]+)\.png"\)\s*\)'
)
src, n = pattern.subn(lambda m: 'QIcon(":/icons/%s.svg")' % m.group(1), src)
print('  replaced fromTheme calls:', n)

# 2) 给原本无图标的 action 补图标
#    格式： m_xxxAction = new QAction(tr("文字"), this);
#    目标： m_xxxAction = new QAction(QIcon(":/icons/xxx.svg"), tr("文字"), this);
ICONS = {
    'm_saveAsAction':        'save',
    'm_exportHtmlAction':    'exporthtml',
    'm_exportPdfAction':     'exportpdf',
    'm_exitAction':          None,          # 退出用系统图标
    'm_replaceAction':       'replace',
    'm_boldAction':          'bold',
    'm_italicAction':        'italic',
    'm_strikethroughAction': 'strikethrough',
    'm_heading1Action':      'heading1',
    'm_heading2Action':      'heading2',
    'm_heading3Action':      'heading3',
    'm_codeAction':          'code',
    'm_codeBlockAction':     'codeblock',
    'm_linkAction':          'link',
    'm_imageAction':         'image',
    'm_tableAction':         'table',
    'm_bulletListAction':    'bulletlist',
    'm_numberedListAction':  'numberedlist',
    'm_blockquoteAction':    'blockquote',
    'm_horizontalRuleAction': 'horizontalrule',
    'm_togglePreviewAction': 'preview',
    'm_toggleSidebarAction': 'sidebar',
    'm_fullscreenAction':    'fullscreen',
    'm_darkModeAction':      'darkmode',
    'm_focusModeAction':     'focusmode',
    'm_zoomInAction':        'zoomin',
    'm_zoomOutAction':       'zoomout',
    'm_zoomResetAction':     'zoomreset',
}

added = 0
for action, icon in ICONS.items():
    if icon is None:
        continue
    # 匹配该 action 的 new QAction(tr("..."), this);
    rx = re.compile(
        r'(%s\s*=\s*new QAction\()tr\("([^"]*)"\)(,\s*this\);)' % re.escape(action)
    )
    m = rx.search(src)
    if not m:
        print('  [skip] not found:', action)
        continue
    replacement = r'%sQIcon(":/icons/%s.svg"), tr("%s")%s' % (
        m.group(1), icon, m.group(2), m.group(3))
    src = src[:m.start()] + replacement + src[m.end():]
    added += 1

print('  actions with icon added:', added)

with io.open(SRC, 'w', encoding='utf-8', newline='\n') as f:
    f.write(src)

# 3) 校验：不应再有 fromTheme
with io.open(SRC, 'r', encoding='utf-8') as f:
    check = f.read()
if 'fromTheme' in check:
    print('  WARNING: fromTheme still present')
else:
    print('  OK: no fromTheme left')
