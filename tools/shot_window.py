#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
启动程序打开指定文件，等待后抓取窗口图像（不依赖前台）。

用 PrintWindow(hwnd, hdc, 2) —— PW_RENDERFULLCONTENT 标志，
可以直接抓到被遮挡或非前台的窗口内容（Electron/Chromium 需要这个标志）。

用法：shot_window.py <exe> <md> <out.png> [等待秒数] [额外参数用|分隔]
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
MD = sys.argv[2]
OUT = sys.argv[3]
WAIT = float(sys.argv[4]) if len(sys.argv) > 4 else 14.0
EXTRA = sys.argv[5].split('|') if len(sys.argv) > 5 and sys.argv[5] else []

user32 = ctypes.windll.user32
gdi32 = ctypes.windll.gdi32
user32.SetProcessDPIAware()
WNDENUMPROC = ctypes.WINFUNCTYPE(wt.BOOL, wt.HWND, wt.LPARAM)

CLEAN_ENV = {k: v for k, v in os.environ.items()
             if k not in ('ELECTRON_RUN_AS_NODE', 'NODE_OPTIONS')}

exe_name = os.path.basename(EXE)
subprocess.run(['taskkill', '/F', '/IM', exe_name], capture_output=True)
time.sleep(1.0)

proc = subprocess.Popen([EXE] + EXTRA + [MD], env=CLEAN_ENV,
                        stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
time.sleep(WAIT)


def find_win(pid, keyword):
    found = [None]

    def cb(h, l):
        wpid = wt.DWORD()
        user32.GetWindowThreadProcessId(h, ctypes.byref(wpid))
        if wpid.value == pid:
            n = user32.GetWindowTextLengthW(h)
            if n > 0:
                b = ctypes.create_unicode_buffer(n + 1)
                user32.GetWindowTextW(h, b, n + 1)
                if keyword in b.value:
                    found[0] = h
        return True

    user32.EnumWindows(WNDENUMPROC(cb), 0)
    return found[0]


hwnd = find_win(proc.pid, os.path.basename(MD))
if not hwnd:
    # 退一步：找该进程任何可见窗口
    def cb2(h, l):
        wpid = wt.DWORD()
        user32.GetWindowThreadProcessId(h, ctypes.byref(wpid))
        if wpid.value == proc.pid and user32.IsWindowVisible(h):
            if user32.GetWindowTextLengthW(h) > 0:
                hwnd_holder.append(h)
        return True

    hwnd_holder = []
    user32.EnumWindows(WNDENUMPROC(cb2), 0)
    hwnd = hwnd_holder[0] if hwnd_holder else None

if not hwnd:
    # 兜底：按标题全局查找（Popen 的 pid 有时拿不到真正的窗口进程）
    kw = os.path.basename(MD)

    def cb3(h, l):
        n = user32.GetWindowTextLengthW(h)
        if n > 0 and user32.IsWindowVisible(h):
            b = ctypes.create_unicode_buffer(n + 1)
            user32.GetWindowTextW(h, b, n + 1)
            if kw in b.value:
                hwnd_holder2.append(h)
        return True

    hwnd_holder2 = []
    user32.EnumWindows(WNDENUMPROC(cb3), 0)
    hwnd = hwnd_holder2[0] if hwnd_holder2 else None

if not hwnd:
    print('FAIL: 找不到窗口')
    proc.terminate()
    sys.exit(1)

r = wt.RECT()
user32.GetWindowRect(hwnd, ctypes.byref(r))
W, H = r.right - r.left, r.bottom - r.top
print('window %dx%d' % (W, H))

hdc_win = user32.GetWindowDC(hwnd)
memdc = gdi32.CreateCompatibleDC(hdc_win)
bmp = gdi32.CreateCompatibleBitmap(hdc_win, W, H)
gdi32.SelectObject(memdc, bmp)
# PW_RENDERFULLCONTENT = 2，Electron/Chromium 需要这个标志
user32.PrintWindow(hwnd, memdc, 2)


class BIH(ctypes.Structure):
    _fields_ = [('s', wt.DWORD), ('w', wt.LONG), ('h', wt.LONG), ('p', wt.WORD),
                ('b', wt.WORD), ('c', wt.DWORD), ('si', wt.DWORD), ('x', wt.LONG),
                ('y', wt.LONG), ('cu', wt.DWORD), ('ci', wt.DWORD)]


bi = BIH()
bi.s = ctypes.sizeof(BIH)
bi.w = W
bi.h = -H
bi.p = 1
bi.b = 32
bi.c = 0
stride = W * 4
buf = ctypes.create_string_buffer(stride * H)
gdi32.GetDIBits(memdc, bmp, 0, H, buf, ctypes.byref(bi), 0)
gdi32.DeleteObject(bmp)
gdi32.DeleteDC(memdc)
user32.ReleaseDC(hwnd, hdc_win)

raw = buf.raw[:stride * H]
rows = []
for y in range(H):
    rr = raw[y * stride:(y + 1) * stride]
    a = bytearray(W * 4)
    for x in range(W):
        a[x * 4] = rr[x * 4 + 2]
        a[x * 4 + 1] = rr[x * 4 + 1]
        a[x * 4 + 2] = rr[x * 4]
        a[x * 4 + 3] = 255
    rows.append(b'\x00' + bytes(a))


def chunk(tag, payload):
    return (struct.pack('>I', len(payload)) + tag + payload +
            struct.pack('>I', zlib.crc32(tag + payload) & 0xFFFFFFFF))


png = b'\x89PNG\r\n\x1a\n'
png += chunk(b'IHDR', struct.pack('>IIBBBBB', W, H, 8, 6, 0, 0, 0))
png += chunk(b'IDAT', zlib.compress(b''.join(rows), 6))
png += chunk(b'IEND', b'')
with open(OUT, 'wb') as f:
    f.write(png)
print('saved %s (%d bytes)' % (OUT, len(png)))

subprocess.run(['taskkill', '/F', '/IM', exe_name], capture_output=True)
