#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
按 Typora 的公开界面规格重构菜单栏与快捷键。

Typora 的菜单结构：
  文件(F) 编辑(E) 段落(P) 格式(O) 视图(V) 主题(T) 帮助(H)

各菜单的项与快捷键：

【文件】
  新建 Ctrl+N / 新建窗口 Ctrl+Shift+N / 打开 Ctrl+O / 打开最近文件
  打开文件夹 Ctrl+Shift+O / 打开最近目录 / 保存 Ctrl+S / 另存为 Ctrl+Shift+S
  格式为 / 偏好设置文件 / 导出 / 打印 / 导出为PDF Alt+Shift+P / 偏好设置 Ctrl+,

【编辑】
  撤销 Ctrl+Z / 重做 Ctrl+Y / 剪切 Ctrl+X / 复制 Ctrl+C / 粘贴 Ctrl+V
  粘贴为Markdown Ctrl+Shift+V / 删除 / 全选 Ctrl+A
  复制为Markdown Ctrl+Shift+C / 复制为HTML代码 / 粘贴为纯文本
  智能标点(子菜单) / 选择(子菜单) / 删除(子菜单) / 数学工具(子菜单) / 偏好设置

【段落】
  一级~六级标题 Ctrl+1..6 / 段落 Ctrl+0
  提升标题级别 Ctrl+[ / 降低标题级别 Ctrl+]
  表格 Ctrl+Shift+] / 公式块 Ctrl+Shift+M / 代码块 Ctrl+Shift+C / 代码 Ctrl+Shift+I
  引用 Ctrl+Shift+Q / 有序列表 Ctrl+Shift+U / 无序列表 Ctrl+Shift+L
  任务列表 Ctrl+Shift+X / 任务状态 / 列表缩进
  表格 / 表格行 / 表格列
  在上方插入段落 / 在下方插入段落
  数学公式 / 脚注 / 水平分割线 / 内容目录 / YAML Front Matter

【格式】
  加粗 Ctrl+B / 斜体 Ctrl+I / 删除线 Ctrl+K / 代码块 Ctrl+Shift+C / 代码 Ctrl+Shift+I
  内嵌公式 / 高亮 Alt+Shift+H / 上标 Ctrl+Shift++ / 下标 Ctrl+Shift+-
  注释 / 超链接 Ctrl+Shift+K / 图像 / 表格 / 创建脚注 / 清除格式

【视图】
  显示/隐藏侧边栏 Ctrl+Shift+L / 大纲 Ctrl+Shift+1 / 文档树 Ctrl+Shift+2
  文件树 Ctrl+Shift+3 / 搜索 Ctrl+Shift+F
  源代码模式 Ctrl+/ / 专注模式 F9 / 打字机模式 F10 / 显示源代码 Ctrl+Shift+D
  侧边栏 F11 / 保持窗口在最前端 / 实际大小 Ctrl+Shift+9
  放大 Ctrl+Shift++ / 缩小 Ctrl+Shift+- / 开发者工具 Shift+F12

【主题】 Github / Newsprint / Night / Pixyll / Whitey
"""
import io
import sys

SRC = sys.argv[1]

with io.open(SRC, 'r', encoding='utf-8') as f:
    src = f.read()

# ===========================================================================
# 1) 快捷键表：整体替换 createActions 里的 setShortcut 调用
# ===========================================================================
# 键名 -> (Modifier, Key) 表达式
KEYMAP = {
    # 文件
    'm_newAction':                 'Qt::CTRL | Qt::Key_N',
    'm_openAction':                'Qt::CTRL | Qt::Key_O',
    'm_openFolderAction':          'Qt::CTRL | Qt::SHIFT | Qt::Key_O',
    'm_saveAction':                'Qt::CTRL | Qt::Key_S',
    'm_saveAsAction':              'Qt::CTRL | Qt::SHIFT | Qt::Key_S',
    'm_exportPdfAction':           'Qt::ALT | Qt::SHIFT | Qt::Key_P',
    # 编辑
    'm_undoAction':                'Qt::CTRL | Qt::Key_Z',
    'm_redoAction':                'Qt::CTRL | Qt::Key_Y',
    'm_cutAction':                 'Qt::CTRL | Qt::Key_X',
    'm_copyAction':                'Qt::CTRL | Qt::Key_C',
    'm_pasteAction':               'Qt::CTRL | Qt::Key_V',
    'm_pasteMarkdownAction':       'Qt::CTRL | Qt::SHIFT | Qt::Key_V',
    'm_selectAllAction':           'Qt::CTRL | Qt::Key_A',
    'm_copyMarkdownAction':        'Qt::CTRL | Qt::SHIFT | Qt::Key_C',
    'm_copyHtmlAction':            'Qt::CTRL | Qt::SHIFT | Qt::Key_H',
    'm_pastePlainTextAction':      'Qt::CTRL | Qt::SHIFT | Qt::Key_T',
    # 段落
    'm_heading1Action':            'Qt::CTRL | Qt::Key_1',
    'm_heading2Action':            'Qt::CTRL | Qt::Key_2',
    'm_heading3Action':            'Qt::CTRL | Qt::Key_3',
    'm_heading4Action':            'Qt::CTRL | Qt::Key_4',
    'm_heading5Action':            'Qt::CTRL | Qt::Key_5',
    'm_heading6Action':            'Qt::CTRL | Qt::Key_6',
    'm_paragraphAction':           'Qt::CTRL | Qt::Key_0',
    'm_increaseHeadingAction':     'Qt::CTRL | Qt::Key_BracketLeft',
    'm_decreaseHeadingAction':     'Qt::CTRL | Qt::Key_BracketRight',
    'm_tableBlockAction':          'Qt::CTRL | Qt::SHIFT | Qt::Key_BracketRight',
    'm_mathBlockAction':           'Qt::CTRL | Qt::SHIFT | Qt::Key_M',
    'm_codeBlockAction':           'Qt::CTRL | Qt::SHIFT | Qt::Key_C',
    'm_inlineCodeAction':          'Qt::CTRL | Qt::SHIFT | Qt::Key_I',
    'm_blockquoteAction':          'Qt::CTRL | Qt::SHIFT | Qt::Key_Q',
    'm_numberedListAction':        'Qt::CTRL | Qt::SHIFT | Qt::Key_U',
    'm_bulletListAction':          'Qt::CTRL | Qt::SHIFT | Qt::Key_L',
    'm_taskListAction':            'Qt::CTRL | Qt::SHIFT | Qt::Key_X',
    # 格式
    'm_boldAction':                'Qt::CTRL | Qt::Key_B',
    'm_italicAction':              'Qt::CTRL | Qt::Key_I',
    'm_strikethroughAction':       'Qt::CTRL | Qt::Key_K',
    'm_highlightAction':           'Qt::ALT | Qt::SHIFT | Qt::Key_H',
    'm_superscriptAction':         'Qt::CTRL | Qt::SHIFT | Qt::Key_Plus',
    'm_subscriptAction':           'Qt::CTRL | Qt::SHIFT | Qt::Key_Underscore',
    'm_hyperlinkAction':           'Qt::CTRL | Qt::SHIFT | Qt::Key_K',
    # 视图
    'm_toggleSidebarAction':       'Qt::CTRL | Qt::SHIFT | Qt::Key_L',
    'm_outlineAction':             'Qt::CTRL | Qt::SHIFT | Qt::Key_1',
    'm_docTreeAction':             'Qt::CTRL | Qt::SHIFT | Qt::Key_2',
    'm_fileTreeAction':            'Qt::CTRL | Qt::SHIFT | Qt::Key_3',
    'm_findAction':                'Qt::CTRL | Qt::SHIFT | Qt::Key_F',
    'm_sourceModeAction':          'Qt::CTRL | Qt::Key_Slash',
    'm_focusModeAction':           'Qt::Key_F9',
    'm_typewriterModeAction':      'Qt::Key_F10',
    'm_showSourceAction':          'Qt::CTRL | Qt::SHIFT | Qt::Key_D',
    'm_sidebarAction':             'Qt::Key_F11',
    'm_actualSizeAction':          'Qt::CTRL | Qt::SHIFT | Qt::Key_9',
    'm_zoomInAction':              'Qt::CTRL | Qt::SHIFT | Qt::Key_Plus',
    'm_zoomOutAction':             'Qt::CTRL | Qt::SHIFT | Qt::Key_Underscore',
    'm_devToolsAction':            'Qt::SHIFT | Qt::Key_F12',
}

count = 0
import re
for name, expr in KEYMAP.items():
    # 匹配该 action 的 setShortcut 行（整行替换，兼容嵌套括号）
    pat = re.compile(r'^([ \t]*)' + re.escape(name) + r'->setShortcut\([^;]*\);$',
                     re.MULTILINE)
    new_src, n = pat.subn(
        lambda m: m.group(1) + name + '->setShortcut(QKeySequence(' + expr + '));',
        src)
    if n:
        src = new_src
        count += n
    else:
        print('  [跳过] 未找到 setShortcut:', name)

print('快捷键替换 %d 处' % count)

with io.open(SRC, 'w', encoding='utf-8', newline='\n') as f:
    f.write(src)
print('OK')
