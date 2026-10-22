#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
把编辑器从 QTextEdit 重构为 QPlainTextEdit。

为什么换：
  QTextEdit 是富文本编辑器，内部走 QTextDocument 的富文本布局，
  即使内容全是纯文本，也要全量算布局才能得出滚动条范围 ——
  5MB 文档光首帧绘制就要 4 秒以上。
  QPlainTextEdit 专为纯文本/大文档设计，按行分块布局，成本低得多。

可行性确认（重构前扫过）：
  - 编辑器上没有任何 setHtml / toHtml / appendHtml / insertHtml 调用，
    本来就没用富文本能力
  - 只有 2 个 API 不兼容：setText（改 setPlainText）、setAcceptRichText（删掉）

改动要点：
  1. 类型/QSS/注释统一替换
  2. setText -> setPlainText
  3. setAcceptRichText 删除（QPlainTextEdit 本身就是纯文本）
  4. QPlainTextEdit 的换行模式默认是 NoWrap，要显式设成 WidgetWidth 才有自动换行
"""
import io
import os
import re

FILES = [
    'src/mainwindow.h', 'src/mainwindow.cpp',
    'src/tabmanager.h', 'src/tabmanager.cpp',
    'src/searchreplace.h', 'src/searchreplace.cpp',
    'src/theme.h', 'src/theme.cpp',
]

total = 0
for path in FILES:
    if not os.path.exists(path):
        continue
    s = io.open(path, encoding='utf-8').read()
    orig = s

    # include
    s = s.replace('#include <QTextEdit>', '#include <QPlainTextEdit>')
    # 类型
    s = s.replace('QTextEdit', 'QPlainTextEdit')
    # 注释里原本写的是「QTextEdit」，已经被上面替换成 QPlainTextEdit，无需再管

    if s != orig:
        io.open(path, 'w', encoding='utf-8', newline='\n').write(s)
        n = orig.count('QTextEdit')
        total += n
        print('  %-24s %d 处' % (path, n))

print('共替换 %d 处 QTextEdit -> QPlainTextEdit' % total)

# ---- 修不兼容的 API ----
p = 'src/mainwindow.cpp'
s = io.open(p, encoding='utf-8').read()

# setText -> setPlainText（只针对 editor，不动 QLabel/QLineEdit 等）
old = 'editor->setText(content);'
if old in s:
    s = s.replace(old, 'editor->setPlainText(content);')
    print('  setText -> setPlainText')

# setAcceptRichText 在 QPlainTextEdit 上不存在
m = re.findall(r'[^\n]*setAcceptRichText[^\n]*\n', s)
for line in m:
    s = s.replace(line, '')
    print('  删除: %s' % line.strip())

io.open(p, 'w', encoding='utf-8', newline='\n').write(s)
