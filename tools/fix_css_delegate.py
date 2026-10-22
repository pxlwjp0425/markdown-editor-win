#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
把旧的 getDefaultCss() 硬编码 CSS 替换为委托主题系统的薄封装。

背景：Typora 风格功能引入了 Theme 模块后，预览区 CSS 由
      Theme::previewCss(themeId) 统一生成（支持 7 套主题），
      旧的 getDefaultCss() 里的两份硬编码 CSS（浅色/深色）
      成为死代码，且与主题体系重复维护。
"""
import io
import re
import sys

path = sys.argv[1]
with io.open(path, 'r', encoding='utf-8') as f:
    src = f.read()

start = src.index('QString MainWindow::getDefaultCss()')
end = src.index('QString MainWindow::getDefaultJs()')

new_impl = '''QString MainWindow::getDefaultCss()
{
    // 预览区 CSS 统一由主题系统生成。
    // 深色模式下若当前主题是浅色主题，自动切到对应的深色变体。
    QString themeId = m_themeId;
    if (m_darkMode) {
        const Theme::Palette pal = Theme::palette(themeId);
        if (!pal.dark) {
            // 浅色主题 + 深色模式：使用该主题的深色对应版本
            if (themeId == Theme::ThemeId::GitHub) {
                themeId = Theme::ThemeId::GitHubDark;
            } else if (themeId == Theme::ThemeId::Default) {
                themeId = Theme::ThemeId::Night;
            }
        }
    }
    return Theme::previewCss(themeId);
}

'''

src = src[:start] + new_impl + src[end:]

with io.open(path, 'w', encoding='utf-8', newline='\n') as f:
    f.write(src)

print('getDefaultCss() 已替换为主题系统委托实现')
print('  移除行数:', src.count('font-family: \'Segoe UI\', \'Microsoft YaHei\'') )
