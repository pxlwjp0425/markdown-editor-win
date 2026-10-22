#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
验证 exe 内嵌图标，并导出为 PNG 便于查看。

用 shell32.ExtractIconExW 从 exe 的资源段取出图标，
再用 DrawIconEx 画到内存 DC，最后转 PNG。

用法：check_exe_icon.py <exe> <out.png> [尺寸]
"""
import ctypes
import ctypes.wintypes as wt
import struct
import sys
import zlib

EXE = sys.argv[1]
OUT = sys.argv[2]
SIZE = int(sys.argv[3]) if len(sys.argv) > 3 else 256

shell32 = ctypes.windll.shell32
user32 = ctypes.windll.user32
gdi32 = ctypes.windll.gdi32
user32.SetProcessDPIAware()

# 先查有多少个图标资源
total = shell32.ExtractIconExW(EXE, -1, None, None, 0)
print('exe 内图标组数量: %d' % total)
if total <= 0:
    print('FAIL: exe 没有内嵌图标')
    sys.exit(1)

large = (wt.HICON * 1)()
small = (wt.HICON * 1)()
got = shell32.ExtractIconExW(EXE, 0, large, small, 1)
hicon = large[0] or small[0]
if not hicon:
    print('FAIL: 提取图标句柄失败')
    sys.exit(1)
print('HICON = %s' % hicon)

# 画到 32 位 DIB
hdc_screen = user32.GetDC(0)
memdc = gdi32.CreateCompatibleDC(hdc_screen)

class BITMAPINFOHEADER(ctypes.Structure):
    _fields_ = [('biSize', wt.DWORD), ('biWidth', wt.LONG), ('biHeight', wt.LONG),
                ('biPlanes', wt.WORD), ('biBitCount', wt.WORD),
                ('biCompression', wt.DWORD), ('biSizeImage', wt.DWORD),
                ('biXPelsPerMeter', wt.LONG), ('biYPelsPerMeter', wt.LONG),
                ('biClrUsed', wt.DWORD), ('biClrImportant', wt.DWORD)]

bi = BITMAPINFOHEADER()
bi.biSize = ctypes.sizeof(BITMAPINFOHEADER)
bi.biWidth = SIZE
bi.biHeight = -SIZE
bi.biPlanes = 1
bi.biBitCount = 32
bi.biCompression = 0

pv = ctypes.c_void_p()
bmp = gdi32.CreateDIBSection(memdc, ctypes.byref(bi), 0,
                             ctypes.byref(pv), None, 0)
gdi32.SelectObject(memdc, bmp)

# 先填白底，方便看轮廓
rect = wt.RECT(0, 0, SIZE, SIZE)
brush = gdi32.CreateSolidBrush(0x00FFFFFF)
user32.FillRect(memdc, ctypes.byref(rect), brush)
gdi32.DeleteObject(brush)

# 用 DI_NORMAL 绘制（含 alpha 混合）
if not user32.DrawIconEx(memdc, 0, 0, hicon, SIZE, SIZE, 0, None, 0x0003):
    print('警告: DrawIconEx 返回 0（仍继续读像素）')

stride = SIZE * 4
buf = ctypes.create_string_buffer(stride * SIZE)
gdi32.GetDIBits(memdc, bmp, 0, SIZE, buf, ctypes.byref(bi), 0)
raw = buf.raw[:stride * SIZE]

rows = []
for y in range(SIZE):
    r = raw[y * stride:(y + 1) * stride]
    a = bytearray(SIZE * 4)
    for x in range(SIZE):
        a[x * 4] = r[x * 4 + 2]
        a[x * 4 + 1] = r[x * 4 + 1]
        a[x * 4 + 2] = r[x * 4]
        a[x * 4 + 3] = 255
    rows.append(b'\x00' + bytes(a))


def chunk(tag, payload):
    return (struct.pack('>I', len(payload)) + tag + payload +
            struct.pack('>I', zlib.crc32(tag + payload) & 0xFFFFFFFF))


png = b'\x89PNG\r\n\x1a\n'
png += chunk(b'IHDR', struct.pack('>IIBBBBB', SIZE, SIZE, 8, 6, 0, 0, 0))
png += chunk(b'IDAT', zlib.compress(b''.join(rows), 6))
png += chunk(b'IEND', b'')
with open(OUT, 'wb') as f:
    f.write(png)

print('saved %s (%d bytes)' % (OUT, len(png)))

gdi32.DeleteObject(bmp)
gdi32.DeleteDC(memdc)
user32.ReleaseDC(0, hdc_screen)
user32.DestroyIcon(hicon)
