#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
规范化 build.ps1 的编码：确保有且仅有一个 UTF-8 BOM，换行为 CRLF。

原因：Windows PowerShell 5.1 读取无 BOM 的 .ps1 时按系统 ANSI 代码页解析，
中文注释/字符串会乱码并引发语法错误。

注意：必须幂等 —— 重复执行不能叠加 BOM。
之前用 utf-8-sig 读取会保留已有 BOM，再以 utf-8-sig 写出就变成双 BOM，
PowerShell 会把第一个 BOM 当正文，导致 "意外的属性 CmdletBinding" 解析错误。
"""
import io
import sys

path = sys.argv[1]

with io.open(path, 'rb') as f:
    raw = f.read()

# 剥离所有已有的 BOM
while raw.startswith(b'\xef\xbb\xbf'):
    raw = raw[3:]

text = raw.decode('utf-8')

# 统一换行为 CRLF
text = text.replace('\r\n', '\n').replace('\r', '\n').replace('\n', '\r\n')

with io.open(path, 'wb') as f:
    f.write(b'\xef\xbb\xbf')
    f.write(text.encode('utf-8'))

with io.open(path, 'rb') as f:
    check = f.read(6)
print('normalized: head =', check[:6], '| single BOM =', check.startswith(b'\xef\xbb\xbf')
      and not check[3:].startswith(b'\xef\xbb\xbf'))
