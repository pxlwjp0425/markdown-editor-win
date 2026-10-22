#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
自检：单独预览模式。

调程序内置的 --self-test-preview-only，验证：
  - 菜单项 QAction 存在且连接正确
  - 触发后状态翻转、勾选同步
  - 编辑器/侧边栏隐藏、预览区可见
  - 再次触发能完全还原
"""
import os
import subprocess
import sys

EXE = sys.argv[1]
OUT = sys.argv[2]
os.makedirs(OUT, exist_ok=True)

r = subprocess.run([EXE, '--self-test-preview-only'],
                   cwd=os.path.dirname(EXE),
                   capture_output=True, text=True, timeout=30,
                   encoding='utf-8', errors='replace')

out = r.stdout.strip()
if not out:
    print('FAIL: 无输出 (exit=%d)' % r.returncode)
    print(r.stderr[:300])
    sys.exit(1)

print(out)

# 解析成字典
# 注意「还原后 mode=0 编辑器可见=1 侧边栏可见=1」一行含多个键值对，
# 所以要按 ' key=' 的边界切，不能简单 split('=')。
kv = {}
for line in out.splitlines():
    for part in line.split(' '):
        if '=' not in part:
            continue
        k, v = part.split('=', 1)
        k = k.strip()
        if k in ('还原后', 'mode'):
            continue
        kv[k] = v.strip()
    if line.startswith('还原后 mode='):
        kv['还原后mode'] = line.split('mode=')[1].split(' ')[0]
        for part in line.split(' '):
            if '可见=' in part:
                key, val = part.split('=', 1)
                kv[key] = val

CHECKS = [
    ('action存在', '1'),
    ('状态翻转', '1'),
    ('勾选同步', '1'),
    ('编辑器隐藏', '1'),
    ('侧边栏隐藏', '1'),
    ('预览区可见', '1'),
    # 从源码模式切入单独预览模式：源码模式要退出、勾选要同步、
    # 语法高亮要恢复、预览要可见
    ('源码模式退出', '1'),
    ('源码勾选同步', '1'),
    ('高亮已恢复', '1'),
    ('源码模式预览可见', '1'),
]

fails = []
for key, expect in CHECKS:
    got = kv.get(key)
    ok = got == expect
    print('  %-6s %-12s = %s' % ('PASS' if ok else 'FAIL', key, got))
    if not ok:
        fails.append(key)

# 还原检查
restored_ok = (kv.get('还原后mode') == '0'
               and kv.get('编辑器可见') == '1'
               and kv.get('侧边栏可见') == '1')
print('  %-6s %-12s' % ('PASS' if restored_ok else 'FAIL', '可还原'))
if not restored_ok:
    fails.append('还原')

if fails:
    print('\n%d 项失败: %s' % (len(fails), ', '.join(fails)))
    sys.exit(1)

print('\n全部 %d 项检查通过' % (len(CHECKS) + 1))
sys.exit(0)
