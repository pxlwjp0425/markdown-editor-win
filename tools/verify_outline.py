#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
隔离测试大纲面板：只创建 OutlinePanel，填充内容，截图。
用于判断问题出在 OutlinePanel 本身还是 MainWindow 的集成。
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
os.makedirs(OUT, exist_ok=True)

user32 = ctypes.windll.user32
gdi32 = ctypes.windll.gdi32
user32.SetProcessDPIAware()

import sys as _s
SAMPLE = _s.argv[3] if len(_s.argv) > 3 else None
_args = [EXE] + ([SAMPLE] if SAMPLE else [])
proc = subprocess.Popen(_args, cwd=os.path.dirname(EXE))
time.sleep(3)

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
time.sleep(1.2)

# 点击「大纲」标签页：侧边栏 tab 大约在窗口左侧 (x=75, y=95)
# 先切到「大纲」标签
for x, y in [(75, 95)]:
    user32.SetCursorPos(80 + x, 50 + y)
    time.sleep(0.2)
    user32.mouse_event(0x0002, 0, 0, 0, 0)  # LEFTDOWN
    time.sleep(0.05)
    user32.mouse_event(0x0004, 0, 0, 0, 0)  # LEFTUP
    time.sleep(0.8)

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
        b = row[x * 4]
        g = row[x * 4 + 1]
        r = row[x * 4 + 2]
        rgba[x * 4] = r
        rgba[x * 4 + 1] = g
        rgba[x * 4 + 2] = b
        rgba[x * 4 + 3] = 255
    rows.append(b'\x00' + bytes(rgba))


def chunk(tag, payload):
    return (struct.pack('>I', len(payload)) + tag + payload +
            struct.pack('>I', zlib.crc32(tag + payload) & 0xFFFFFFFF))


png = b'\x89PNG\r\n\x1a\n'
png += chunk(b'IHDR', struct.pack('>IIBBBBB', w, h, 8, 6, 0, 0, 0))
png += chunk(b'IDAT', zlib.compress(b''.join(rows), 6))
png += chunk(b'IEND', b'')

p = os.path.join(OUT, 'outline-tab.png')
with open(p, 'wb') as f:
    f.write(png)
print('screenshot ->', p, os.path.getsize(p))

proc.terminate()
