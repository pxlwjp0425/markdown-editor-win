#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
给 verify_preview_only.py 追加「通过 WM_COMMAND 触发菜单项」的辅助函数。

背景：沙箱环境下模拟键盘送不到前台窗口（Windows 前台锁定），
但菜单命令可以直接发 WM_COMMAND，走的是和用户点击完全相同的代码路径。
"""
import io

TARGET = 'tools/verify_preview_only.py'

FUNC = '''

def click_menu_item(h, menu_path, item_text):
    """通过 WM_COMMAND 触发菜单项。

    键盘模拟在沙箱环境里拿不到前台焦点，但菜单命令可以直接发消息，
    走的是和用户点击完全相同的代码路径。
    步骤：HMENU -> 逐级 GetSubMenu -> GetMenuString 找到项 -> 发 WM_COMMAND。
    """
    WM_COMMAND = 0x0111
    MF_BYPOSITION = 0x400

    hmenu = user32.GetMenu(h)
    if not hmenu:
        print('no menu')
        return False

    cur = hmenu
    for idx in menu_path:
        cur = user32.GetSubMenu(cur, idx)
        if not cur:
            print('no submenu at', idx)
            return False

    count = user32.GetMenuItemCount(cur)
    for i in range(count):
        buf = ctypes.create_unicode_buffer(256)
        user32.GetMenuStringW(cur, i, buf, 256, MF_BYPOSITION)
        text = buf.value.replace('&', '').strip()
        if text == item_text:
            cmd_id = user32.GetMenuItemID(cur, i)
            if cmd_id == 0 or cmd_id == 0xFFFFFFFF:
                print('item is submenu:', text)
                return False
            user32.PostMessageW(h, WM_COMMAND, cmd_id, 0)
            time.sleep(0.8)
            return True

    print('menu item not found; available:')
    for i in range(count):
        buf = ctypes.create_unicode_buffer(256)
        user32.GetMenuStringW(cur, i, buf, 256, MF_BYPOSITION)
        print('  ', i, repr(buf.value))
    return False
'''

with io.open(TARGET, 'r', encoding='utf-8') as f:
    src = f.read()

if 'def click_menu_item' in src:
    print('已存在，跳过')
else:
    # 插到 send_ctrl_shift_p 之后、shot 定义之前
    marker = '\ndef shot(name):'
    if marker in src:
        src = src.replace(marker, FUNC + marker, 1)
    else:
        src = src.rstrip() + '\n' + FUNC

    with io.open(TARGET, 'w', encoding='utf-8', newline='\n') as f:
        f.write(src)
    print('已插入 click_menu_item')
