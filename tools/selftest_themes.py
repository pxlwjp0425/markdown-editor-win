#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
主题系统验证：导出 HTML 时指定主题，检查配色是否随之改变。

主题通过 --theme 参数指定（需程序支持），或直接检查各主题的 CSS 生成。
"""
import os
import subprocess
import sys

EXE = sys.argv[1]
OUT = sys.argv[2]
os.makedirs(OUT, exist_ok=True)

SAMPLE = os.path.join(OUT, 'theme-test.md')
with open(SAMPLE, 'w', encoding='utf-8') as f:
    f.write("# 标题\n\n正文内容。\n")

# 各主题的预期特征色（取自 Typora 主题定义）
# 注意：QColor::name() 输出小写，比较时统一转小写
THEMES = {
    'github':      ('#ffffff', '#333333', '#4183c4'),
    'github-dark': ('#0d1117', '#c9d1d9', '#58a6ff'),
    'night':       ('#363b40', '#b8bfc6', '#6dc1e7'),
    'newsprint':   ('#fffff8', '#333333', '#4a4a4a'),
    'pixyll':      ('#ffffff', '#333333', '#3e8338'),
    'whitey':      ('#ffffff', '#4a4a4a', '#4a4a4a'),
    'default':     ('#ffffff', '#333333', '#3498db'),
}

fails = []
for theme, (bg, fg, link) in THEMES.items():
    out = os.path.join(OUT, 'theme-%s.html' % theme)
    if os.path.exists(out):
        os.remove(out)
    r = subprocess.run([EXE, SAMPLE, '--theme', theme, '--export-html', out],
                       cwd=os.path.dirname(EXE),
                       capture_output=True, text=True, timeout=30,
                       encoding='utf-8', errors='replace')
    if not os.path.exists(out):
        print('  FAIL   %-12s 未生成 HTML (exit=%d) %s' % (theme, r.returncode, r.stderr[:120]))
        fails.append(theme)
        continue

    with open(out, encoding='utf-8') as f:
        html = f.read()

    checks = []
    checks.append(('--bg-color: %s;' % bg, '背景色 %s' % bg))
    checks.append(('--text-color: %s;' % fg, '文字色 %s' % fg))
    checks.append(('--link-color: %s;' % link, '链接色 %s' % link))

    bad = [label for needle, label in checks if needle not in html]
    if bad:
        print('  FAIL   %-12s %s' % (theme, ', '.join(bad)))
        fails.append(theme)
    else:
        print('  PASS   %-12s 配色正确' % theme)

if fails:
    print('\n%d 个主题失败: %s' % (len(fails), ', '.join(fails)))
    sys.exit(1)

print('\n全部 %d 个主题检查通过' % len(THEMES))
sys.exit(0)
