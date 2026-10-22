#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
扫描 mainwindow.cpp 中的快捷键定义，找出重复绑定。

移植时容易漏掉的坑：原项目里多个 action 绑定了同一个按键，
在 Linux 下不显眼（无全局占用），在 Windows 上会真实冲突。
"""
import io
import re
import sys
from collections import defaultdict

SRC = sys.argv[1]

with io.open(SRC, 'r', encoding='utf-8') as f:
    lines = f.readlines()

# 形如： m_xxxAction->setShortcut(QKeySequence::New);
#       m_xxxAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_B));
STD = {
    'QKeySequence::New': 'Ctrl+N',
    'QKeySequence::Open': 'Ctrl+O',
    'QKeySequence::Save': 'Ctrl+S',
    'QKeySequence::SaveAs': 'Ctrl+Shift+S',
    'QKeySequence::Quit': 'Ctrl+Q',
    'QKeySequence::Undo': 'Ctrl+Z',
    'QKeySequence::Redo': 'Ctrl+Y',
    'QKeySequence::Cut': 'Ctrl+X',
    'QKeySequence::Copy': 'Ctrl+C',
    'QKeySequence::Paste': 'Ctrl+V',
    'QKeySequence::Find': 'Ctrl+F',
    'QKeySequence::SelectAll': 'Ctrl+A',
    'QKeySequence::ZoomIn': 'Ctrl++',
    'QKeySequence::ZoomOut': 'Ctrl+-',
    'QKeySequence::FullScreen': 'F11',
}

KEYNAME = {
    'Key_A': 'A', 'Key_B': 'B', 'Key_C': 'C', 'Key_D': 'D', 'Key_E': 'E',
    'Key_F': 'F', 'Key_G': 'G', 'Key_H': 'H', 'Key_I': 'I', 'Key_J': 'J',
    'Key_K': 'K', 'Key_L': 'L', 'Key_M': 'M', 'Key_N': 'N', 'Key_O': 'O',
    'Key_P': 'P', 'Key_Q': 'Q', 'Key_R': 'R', 'Key_S': 'S', 'Key_T': 'T',
    'Key_U': 'U', 'Key_V': 'V', 'Key_W': 'W', 'Key_X': 'X', 'Key_Y': 'Y',
    'Key_Z': 'Z', 'Key_0': '0', 'Key_1': '1', 'Key_2': '2', 'Key_3': '3',
    'Key_4': '4', 'Key_5': '5', 'Key_6': '6', 'Key_7': '7', 'Key_8': '8',
    'Key_9': '9',
}

pat = re.compile(r'(m_\w+Action)->setShortcut\((.+?)\);')

mapping = defaultdict(list)

for idx, line in enumerate(lines, 1):
    m = pat.search(line)
    if not m:
        continue
    action, expr = m.group(1), m.group(2)
    expr = expr.strip()

    if expr in STD:
        key = STD[expr]
    else:
        # QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_X)
        mods = []
        if 'Qt::CTRL' in expr:
            mods.append('Ctrl')
        if 'Qt::SHIFT' in expr:
            mods.append('Shift')
        if 'Qt::ALT' in expr:
            mods.append('Alt')
        km = re.search(r'Qt::(Key_\w+)', expr)
        if km:
            keyname = KEYNAME.get(km.group(1), km.group(1))
            key = '+'.join(mods + [keyname])
        else:
            key = expr
    mapping[key].append((action, idx))

conflicts = {k: v for k, v in mapping.items() if len(v) > 1}

if conflicts:
    print('发现 %d 组快捷键冲突：' % len(conflicts))
    for k, v in sorted(conflicts.items()):
        print('  %s:' % k)
        for action, ln in v:
            print('      %-30s (行 %d)' % (action, ln))
    sys.exit(1)
else:
    print('OK: 共 %d 个快捷键，无冲突' % len(mapping))
    for k in sorted(mapping):
        print('  %-18s %s' % (k, mapping[k][0][0]))
    sys.exit(0)
