#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
抓取「文件 > 导出」弹出菜单的截图。

注意：弹出菜单是独立顶层窗口，PrintWindow 抓主窗口抓不到它，
所以这里改用 BitBlt 直接抓整个屏幕的对应区域。
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
os.makedirs(OUT, exist_ok=True)

user32 = ctypes.windll.user32
gdi32 = ctypes.windll.gdi32
user32.SetProcessDPIAware()

args = [EXE] + ([SAMPLE] if SAMPLE else []) + ['--show-export-menu']
proc = subprocess.Popen(args, cwd=os.path.dirname(EXE))
time.sleep(4.0)   # 等程序起来 + 菜单自动弹出（钩子是 1200ms 后弹）

# ---- 抓全屏 ----
screen_w = user32.GetSystemMetrics(0)
screen_h = user32.GetSystemMetrics(1)

hdc_screen = user32.GetDC(0)
memdc = gdi32.CreateCompatibleDC(hdc_screen)
bmp = gdi32.CreateCompatibleBitmap(hdc_screen, screen_w, screen_h)
gdi32.SelectObject(memdc, bmp)
gdi32.BitBlt(memdc, 0, 0, screen_w, screen_h, hdc_screen, 0, 0, 0x00CC0020)


class BIH(ctypes.Structure):
    _fields_ = [('biSize', wt.DWORD), ('biWidth', wt.LONG), ('biHeight', wt.LONG),
                ('biPlanes', wt.WORD), ('biBitCount', wt.WORD), ('biCompression', wt.DWORD),
                ('biSizeImage', wt.DWORD), ('biXPelsPerMeter', wt.LONG),
                ('biYPelsPerMeter', wt.LONG), ('biClrUsed', wt.DWORD),
                ('biClrImportant', wt.DWORD)]


bi = BIH()
bi.biSize = ctypes.sizeof(BIH)
bi.biWidth = screen_w
bi.biHeight = -screen_h
bi.biPlanes = 1
bi.biBitCount = 32
bi.biCompression = 0
stride = screen_w * 4
buf = ctypes.create_string_buffer(stride * screen_h)
gdi32.GetDIBits(memdc, bmp, 0, screen_h, buf, ctypes.byref(bi), 0)
gdi32.DeleteObject(bmp)
gdi32.DeleteDC(memdc)
user32.ReleaseDC(0, hdc_screen)

raw = buf.raw[:stride * screen_h]
rows = []
for y in range(screen_h):
    row = raw[y * stride:(y + 1) * stride]
    rgba = bytearray(screen_w * 4)
    for x in range(screen_w):
        rgba[x * 4] = row[x * 4 + 2]
        rgba[x * 4 + 1] = row[x * 4 + 1]
        rgba[x * 4 + 2] = row[x * 4]
        rgba[x * 4 + 3] = 255
    rows.append(b'\x00' + bytes(rgba))


def chunk(tag, payload):
    return (struct.pack('>I', len(payload)) + tag + payload +
            struct.pack('>I', zlib.crc32(tag + payload) & 0xFFFFFFFF))


png = b'\x89PNG\r\n\x1a\n'
png += chunk(b'IHDR', struct.pack('>IIBBBBB', screen_w, screen_h, 8, 6, 0, 0, 0))
png += chunk(b'IDAT', zlib.compress(b''.join(rows), 6))
png += chunk(b'IEND', b'')

# 裁掉主窗口区域（左上角 1500x1000 足够覆盖菜单）
# 上面的全屏图先存，再按需裁剪
path_full = os.path.join(OUT, 'export_menu_full.png')
with open(path_full, 'wb') as f:
    f.write(png)
print('full screen ->', path_full, os.path.getsize(path_full))

proc.terminate()
time.sleep(0.3)
