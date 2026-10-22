#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
验证「单独预览模式」：启动 -> 截图 -> 按 Ctrl+Shift+P -> 再截图。
"""
import ctypes
import ctypes.wintypes as wt
import os
import struct
import subprocess
import sys
import time
import zlib

EXE = sys.argv[1]
OUT = sys.argv[2]
SAMPLE = sys.argv[3] if len(sys.argv) > 3 else None
# 额外参数：--preview-only 表示用命令行直接进入预览模式（不走快捷键）
EXTRA = sys.argv[4:]
os.makedirs(OUT, exist_ok=True)

user32 = ctypes.windll.user32
gdi32 = ctypes.windll.gdi32
user32.SetProcessDPIAware()

args = [EXE] + ([SAMPLE] if SAMPLE else []) + EXTRA
proc = subprocess.Popen(args, cwd=os.path.dirname(EXE))
time.sleep(3.5)

found = []


@ctypes.WINFUNCTYPE(ctypes.c_bool, wt.HWND, wt.LPARAM)
def cb(h, l):
    p = wt.DWORD()
    user32.GetWindowThreadProcessId(h, ctypes.byref(p))
    if p.value == proc.pid and user32.IsWindowVisible(h):
        n = user32.GetWindowTextLengthW(h)
        if n:
            b = ctypes.create_unicode_buffer(n + 1)
            user32.GetWindowTextW(h, b, n + 1)
            if 'Markdown' in b.value:
                found.append((h, b.value))
                return False
    return True


user32.EnumWindows(cb, 0)
if not found:
    print('window not found')
    proc.kill()
    sys.exit(1)

hwnd = found[0][0]
user32.SetWindowPos(hwnd, 0, 80, 50, 1400, 900, 0x0004)
time.sleep(1.0)


def focus_window(h):
    """把窗口设为前台。

    Windows 的前台锁定策略会拒绝 SetForegroundWindow，
    常见绕法是先按一次 Alt（系统认为用户有输入意图），
    或者 AttachThreadInput 借用前台线程的输入队列。
    这里做多轮重试，提高成功率。
    """
    VK_MENU = 0x12
    for _ in range(8):
        user32.keybd_event(VK_MENU, 0, 0, 0)
        user32.keybd_event(VK_MENU, 0, 0x0002, 0)
        user32.SetForegroundWindow(h)

        fg = user32.GetForegroundWindow()
        if fg:
            cur = ctypes.windll.kernel32.GetCurrentThreadId()
            fg_tid = wt.DWORD()
            user32.GetWindowThreadProcessId(fg, ctypes.byref(fg_tid))
            user32.AttachThreadInput(cur, fg_tid.value, True)
            user32.SetForegroundWindow(h)
            user32.SetFocus(h)
            user32.BringWindowToTop(h)
            user32.AttachThreadInput(cur, fg_tid.value, False)
        time.sleep(0.25)
        if user32.GetForegroundWindow() == h:
            return True
    return False


def click_title_bar(h):
    """在标题栏上点一下 —— 真实鼠标点击能可靠地让窗口获得焦点与激活状态。"""
    rect = wt.RECT()
    user32.GetWindowRect(h, ctypes.byref(rect))
    x = (rect.left + rect.right) // 2
    y = rect.top + 15
    user32.SetCursorPos(x, y)
    time.sleep(0.15)
    user32.mouse_event(0x0002, 0, 0, 0, 0)   # LEFTDOWN
    time.sleep(0.05)
    user32.mouse_event(0x0004, 0, 0, 0, 0)   # LEFTUP
    time.sleep(0.3)


def send_ctrl_shift_p():
    """用 SendInput 发组合键（比 keybd_event 可靠）。"""
    KEYEVENTF_KEYUP = 0x0002
    INPUT_KEYBOARD = 1
    VK_CONTROL, VK_SHIFT, VK_P = 0x11, 0x10, 0x50

    class KEYBDINPUT(ctypes.Structure):
        _fields_ = [('wVk', wt.WORD), ('wScan', wt.WORD), ('dwFlags', wt.DWORD),
                    ('time', wt.DWORD), ('dwExtraInfo', ctypes.POINTER(wt.ULONG))]

    class INPUT(ctypes.Structure):
        class _U(ctypes.Union):
            _fields_ = [('ki', KEYBDINPUT), ('padding', ctypes.c_byte * 24)]
        _anonymous_ = ('u',)
        _fields_ = [('type', wt.DWORD), ('u', _U)]

    def key(vk, up=False):
        ki = KEYBDINPUT()
        ki.wVk = vk
        ki.dwFlags = KEYEVENTF_KEYUP if up else 0
        inp = INPUT()
        inp.type = INPUT_KEYBOARD
        inp.ki = ki
        return inp

    seq = [key(VK_CONTROL), key(VK_SHIFT), key(VK_P),
           key(VK_P, True), key(VK_SHIFT, True), key(VK_CONTROL, True)]
    arr = (INPUT * len(seq))(*seq)
    user32.SendInput(len(seq), arr, ctypes.sizeof(INPUT))
    time.sleep(0.12)



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

def shot(name):
    rect = wt.RECT()
    user32.GetWindowRect(hwnd, ctypes.byref(rect))
    w = rect.right - rect.left
    h = rect.bottom - rect.top
    hdc = user32.GetWindowDC(hwnd)
    memdc = gdi32.CreateCompatibleDC(hdc)
    bmp = gdi32.CreateCompatibleBitmap(hdc, w, h)
    gdi32.SelectObject(memdc, bmp)
    user32.PrintWindow(hwnd, memdc, 2)

    class BIH(ctypes.Structure):
        _fields_ = [('biSize', wt.DWORD), ('biWidth', wt.LONG), ('biHeight', wt.LONG),
                    ('biPlanes', wt.WORD), ('biBitCount', wt.WORD), ('biCompression', wt.DWORD),
                    ('biSizeImage', wt.DWORD), ('biXPelsPerMeter', wt.LONG),
                    ('biYPelsPerMeter', wt.LONG), ('biClrUsed', wt.DWORD), ('biClrImportant', wt.DWORD)]

    bi = BIH()
    bi.biSize = ctypes.sizeof(BIH)
    bi.biWidth = w
    bi.biHeight = -h
    bi.biPlanes = 1
    bi.biBitCount = 32
    bi.biCompression = 0
    stride = ((w * 32 + 31) // 32) * 4
    buf = ctypes.create_string_buffer(stride * h)
    gdi32.GetDIBits(memdc, bmp, 0, h, buf, ctypes.byref(bi), 0)
    gdi32.DeleteObject(bmp)
    gdi32.DeleteDC(memdc)
    user32.ReleaseDC(hwnd, hdc)

    raw = buf.raw[:stride * h]
    rows = []
    for y in range(h):
        row = raw[y * stride:(y + 1) * stride]
        rgba = bytearray(w * 4)
        for x in range(w):
            rgba[x * 4] = row[x * 4 + 2]
            rgba[x * 4 + 1] = row[x * 4 + 1]
            rgba[x * 4 + 2] = row[x * 4]
            rgba[x * 4 + 3] = 255
        rows.append(b'\x00' + bytes(rgba))

    def chunk(tag, payload):
        return (struct.pack('>I', len(payload)) + tag + payload +
                struct.pack('>I', zlib.crc32(tag + payload) & 0xFFFFFFFF))

    png = b'\x89PNG\r\n\x1a\n'
    png += chunk(b'IHDR', struct.pack('>IIBBBBB', w, h, 8, 6, 0, 0, 0))
    png += chunk(b'IDAT', zlib.compress(b''.join(rows), 6))
    png += chunk(b'IEND', b'')

    p = os.path.join(OUT, name)
    with open(p, 'wb') as f:
        f.write(png)
    print('screenshot ->', p, os.path.getsize(p))


def send_ctrl_shift_p():
    # Ctrl+Shift+P
    KEYEVENTF_KEYUP = 0x0002
    VK_CONTROL, VK_SHIFT, VK_P = 0x11, 0x10, 0x50
    user32.keybd_event(VK_CONTROL, 0, 0, 0)
    user32.keybd_event(VK_SHIFT, 0, 0, 0)
    user32.keybd_event(VK_P, 0, 0, 0)
    time.sleep(0.05)
    user32.keybd_event(VK_P, 0, KEYEVENTF_KEYUP, 0)
    user32.keybd_event(VK_SHIFT, 0, KEYEVENTF_KEYUP, 0)
    user32.keybd_event(VK_CONTROL, 0, KEYEVENTF_KEYUP, 0)


if '--preview-only' in EXTRA:
    # 已经通过命令行进入预览模式，直接截图
    time.sleep(1.5)
    shot('preview_only.png')
else:
    shot('preview_before.png')
    # 优先用菜单命令触发（沙箱里键盘送不到前台窗口）
    # 菜单结构：文件 编辑 格式 视图 帮助 —— 「视图」是索引 3
    ok = click_menu_item(hwnd, [3], '单独预览模式')
    print('menu trigger:', ok)
    time.sleep(1.0)
    shot('preview_only.png')
    ok2 = click_menu_item(hwnd, [3], '单独预览模式')
    print('menu trigger back:', ok2)
    time.sleep(1.0)
    shot('preview_restored.png')

proc.terminate()
