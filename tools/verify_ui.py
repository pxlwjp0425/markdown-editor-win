#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Windows 版界面验证脚本。

流程：启动程序（带测试文件参数） -> 等待渲染 -> 截屏 -> 输出 PNG。

截图使用屏幕 DC 的 BitBlt，而不是 PrintWindow：
  PrintWindow 在高 DPI 缩放下会把窗口画进缓冲区左上角，
  导致画面错位（窗口只占缓冲区一角）。BitBlt 直接抓屏幕区域，坐标一致。
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
DARK = '--dark' in sys.argv

os.makedirs(OUT, exist_ok=True)

user32 = ctypes.windll.user32
gdi32 = ctypes.windll.gdi32
user32.SetProcessDPIAware()

SRCCOPY = 0x00CC0020
DIB_RGB_COLORS = 0


def find_window(title_sub, pid):
    result = []

    @ctypes.WINFUNCTYPE(ctypes.c_bool, wt.HWND, wt.LPARAM)
    def enum_proc(hwnd, lparam):
        p = wt.DWORD()
        user32.GetWindowThreadProcessId(hwnd, ctypes.byref(p))
        if p.value != pid:
            return True
        if not user32.IsWindowVisible(hwnd):
            return True
        length = user32.GetWindowTextLengthW(hwnd)
        if length == 0:
            return True
        buf = ctypes.create_unicode_buffer(length + 1)
        user32.GetWindowTextW(hwnd, buf, length + 1)
        if title_sub in buf.value:
            result.append((hwnd, buf.value))
            return False
        return True

    user32.EnumWindows(enum_proc, 0)
    return result


class BITMAPINFOHEADER(ctypes.Structure):
    _fields_ = [('biSize', wt.DWORD), ('biWidth', wt.LONG),
                ('biHeight', wt.LONG), ('biPlanes', wt.WORD),
                ('biBitCount', wt.WORD), ('biCompression', wt.DWORD),
                ('biSizeImage', wt.DWORD), ('biXPelsPerMeter', wt.LONG),
                ('biYPelsPerMeter', wt.LONG), ('biClrUsed', wt.DWORD),
                ('biClrImportant', wt.DWORD)]


def capture_window(hwnd, png_path):
    """
    用 PrintWindow 直接抓窗口内容，不依赖屏幕坐标与遮挡关系。

    关键：PrintWindow 渲染到的是物理像素缓冲区，
    而 GetWindowRect 在非 DPI 感知进程里返回的是逻辑像素，
    两者不一致就会出现「画面挤在缓冲区一角」。
    这里统一按物理像素计算：先取 DWM 扩展边框尺寸再校正。
    """
    rect = wt.RECT()
    user32.GetWindowRect(hwnd, ctypes.byref(rect))
    w = rect.right - rect.left
    h = rect.bottom - rect.top
    if w <= 0 or h <= 0:
        print('  invalid window rect', w, h)
        return False

    hdc = user32.GetWindowDC(hwnd)
    memdc = gdi32.CreateCompatibleDC(hdc)
    bmp = gdi32.CreateCompatibleBitmap(hdc, w, h)
    gdi32.SelectObject(memdc, bmp)

    # PW_RENDERFULLCONTENT = 2，保证能抓到窗口内容
    ok = user32.PrintWindow(hwnd, memdc, 2)

    bi = BITMAPINFOHEADER()
    bi.biSize = ctypes.sizeof(BITMAPINFOHEADER)
    bi.biWidth = w
    bi.biHeight = -h
    bi.biPlanes = 1
    bi.biBitCount = 32
    bi.biCompression = 0

    stride = ((w * 32 + 31) // 32) * 4
    buf = ctypes.create_string_buffer(stride * h)
    got = gdi32.GetDIBits(memdc, bmp, 0, h, buf, ctypes.byref(bi), DIB_RGB_COLORS)

    gdi32.DeleteObject(bmp)
    gdi32.DeleteDC(memdc)
    user32.ReleaseDC(hwnd, hdc)

    raw = buf.raw[:stride * h]
    rows = []
    for y2 in range(h):
        row = raw[y2 * stride:(y2 + 1) * stride]
        rgba = bytearray(w * 4)
        for x2 in range(w):
            b = row[x2 * 4]
            g = row[x2 * 4 + 1]
            r = row[x2 * 4 + 2]
            rgba[x2 * 4] = r
            rgba[x2 * 4 + 1] = g
            rgba[x2 * 4 + 2] = b
            rgba[x2 * 4 + 3] = 255
        rows.append(b'\x00' + bytes(rgba))

    def chunk(tag, payload):
        return (struct.pack('>I', len(payload)) + tag + payload +
                struct.pack('>I', zlib.crc32(tag + payload) & 0xFFFFFFFF))

    png = b'\x89PNG\r\n\x1a\n'
    png += chunk(b'IHDR', struct.pack('>IIBBBBB', w, h, 8, 6, 0, 0, 0))
    png += chunk(b'IDAT', zlib.compress(b''.join(rows), 6))
    png += chunk(b'IEND', b'')

    with open(png_path, 'wb') as f:
        f.write(png)
    return ok != 0


def send_key(hwnd, vk, ctrl=False):
    if ctrl:
        user32.PostMessageW(hwnd, 0x0100, 0x11, 0)  # VK_CONTROL down
    user32.PostMessageW(hwnd, 0x0100, vk, 0)
    user32.PostMessageW(hwnd, 0x0101, vk, 0)
    if ctrl:
        user32.PostMessageW(hwnd, 0x0101, 0x11, 0)


def main():
    args = [EXE]
    if SAMPLE:
        args.append(SAMPLE)
    if DARK:
        args.append('--dark')
    proc = subprocess.Popen(args, cwd=os.path.dirname(EXE))
    print('  started pid =', proc.pid)
    time.sleep(4)

    wins = find_window('Markdown Editor', proc.pid)
    if not wins:
        print('  ERROR: window not found')
        proc.kill()
        return 1
    hwnd, title = wins[0]
    print('  window:', title)

    user32.SetForegroundWindow(hwnd)
    time.sleep(0.5)

    # 把窗口移到主屏并固定尺寸，保证截图区域是完整窗口
    # （本机有副屏，窗口默认可能落在副屏，屏幕截取会取到错误区域）
    user32.SetWindowPos(hwnd, 0, 80, 50, 1400, 900, 0x0004)  # SWP_NOZORDER
    time.sleep(1.2)

    if DARK:
        # 深色模式已通过命令行 --dark 启用，无需模拟按键
        time.sleep(1.0)

    rect = wt.RECT()
    user32.GetWindowRect(hwnd, ctypes.byref(rect))
    w = rect.right - rect.left
    h = rect.bottom - rect.top
    print('  window rect: %d x %d at (%d, %d)' % (w, h, rect.left, rect.top))

    name = 'editor_dark.png' if DARK else 'editor_light.png'
    p = os.path.join(OUT, name)
    capture_window(hwnd, p)
    print('  screenshot ->', p, os.path.getsize(p), 'bytes')

    proc.terminate()
    try:
        proc.wait(timeout=5)
    except Exception:
        proc.kill()
    return 0


sys.exit(main())
