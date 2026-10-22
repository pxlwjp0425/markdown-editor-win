#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
通用的「打开文件到可交互」外部测量。

对任意 GUI 程序都用同一套标准，保证对比公平：
  t_title  主窗口标题包含目标文件名（文件已读入、窗口已出现）
  t_idle   目标进程 CPU 时间连续稳定（渲染完成、可交互）

关键点：
  1. 必须清掉 ELECTRON_RUN_AS_NODE / NODE_OPTIONS ——
     这些是当前 shell 继承来的变量，会让 Electron 应用退化成 Node 模式
     （表现为把 .md 当 JS 脚本执行而报 SyntaxError）。
  2. 每次测量前确保进程完全退出（冷启动）。
  3. 取多轮最优值（OS 调度噪声大）。

用法：
  bench_open_external.py <exe> <md文件> [--rounds N] [--args "a|b|c"] [--label 名称]
"""
import ctypes
import ctypes.wintypes as wt
import os
import subprocess
import sys
import time

user32 = ctypes.windll.user32
kernel32 = ctypes.windll.kernel32
user32.SetProcessDPIAware()

WNDENUMPROC = ctypes.WINFUNCTYPE(wt.BOOL, wt.HWND, wt.LPARAM)

# ---- 参数解析 ----
argv = sys.argv[1:]
if len(argv) < 2:
    print(__doc__)
    sys.exit(1)

EXE, MD = argv[0], argv[1]
ROUNDS = 3
EXTRA = []
LABEL = os.path.splitext(os.path.basename(EXE))[0]
KILL_NAME = os.path.basename(EXE)

i = 2
while i < len(argv):
    if argv[i] == '--rounds':
        ROUNDS = int(argv[i + 1]); i += 2
    elif argv[i] == '--args':
        EXTRA = argv[i + 1].split('|'); i += 2
    elif argv[i] == '--label':
        LABEL = argv[i + 1]; i += 2
    elif argv[i] == '--kill':
        KILL_NAME = argv[i + 1]; i += 2
    else:
        i += 1

BASENAME = os.path.basename(MD)

# 干净的子进程环境：去掉会把 Electron 拉进 Node 模式的变量
CLEAN_ENV = {k: v for k, v in os.environ.items()
             if k not in ('ELECTRON_RUN_AS_NODE', 'NODE_OPTIONS')}


def windows_for_pid(pid):
    found = []

    def cb(hwnd, lparam):
        wpid = wt.DWORD()
        user32.GetWindowThreadProcessId(hwnd, ctypes.byref(wpid))
        if wpid.value == pid and user32.IsWindowVisible(hwnd):
            n = user32.GetWindowTextLengthW(hwnd)
            if n > 0:
                buf = ctypes.create_unicode_buffer(n + 1)
                user32.GetWindowTextW(hwnd, buf, n + 1)
                found.append((hwnd, buf.value))
        return True

    user32.EnumWindows(WNDENUMPROC(cb), 0)
    return found


def cpu_seconds(pid):
    h = kernel32.OpenProcess(0x0400 | 0x0010, False, pid)
    if not h:
        return None
    try:
        c, e, k, u = wt.FILETIME(), wt.FILETIME(), wt.FILETIME(), wt.FILETIME()
        if not kernel32.GetProcessTimes(h, ctypes.byref(c), ctypes.byref(e),
                                        ctypes.byref(k), ctypes.byref(u)):
            return None
        v = lambda ft: (ft.dwHighDateTime << 32) | ft.dwLowDateTime
        return (v(k) + v(u)) / 1e7
    finally:
        kernel32.CloseHandle(h)


def kill_all():
    subprocess.run(['taskkill', '/F', '/IM', KILL_NAME],
                   capture_output=True)
    time.sleep(1.0)


results = []

for r in range(ROUNDS):
    kill_all()

    t0 = time.perf_counter()
    proc = subprocess.Popen([EXE] + EXTRA + [MD], env=CLEAN_ENV,
                            stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)

    t_title = None
    deadline = t0 + 90
    idle_start = None
    last_cpu = 0.0

    while time.perf_counter() < deadline:
        now = time.perf_counter()

        if t_title is None:
            for hwnd, title in windows_for_pid(proc.pid):
                if BASENAME in title:
                    t_title = now - t0
                    break

        if t_title is not None:
            cpu = cpu_seconds(proc.pid)
            if cpu is not None:
                if abs(cpu - last_cpu) < 0.02:
                    if idle_start is None:
                        idle_start = now
                    elif now - idle_start > 0.7:
                        break
                else:
                    idle_start = None
                last_cpu = cpu
        time.sleep(0.03)

    t_idle = time.perf_counter() - t0
    results.append((t_title, t_idle))
    print('  round %d: title=%s idle=%.2fs' % (
        r + 1,
        ('%.2fs' % t_title) if t_title else 'TIMEOUT',
        t_idle))

    try:
        proc.terminate()
    except Exception:
        pass
    kill_all()

print()
print('=== %s ===' % LABEL)
print('  文件: %s (%.2f MB)' % (BASENAME, os.path.getsize(MD) / 1024 / 1024))

titles = [t for t, _ in results if t]
idles = [t for _, t in results]

if titles:
    print('  标题就绪  best=%.2fs  avg=%.2fs' % (min(titles), sum(titles) / len(titles)))
print('  渲染完成  best=%.2fs  avg=%.2fs' % (min(idles), sum(idles) / len(idles)))
print('BEST_IDLE=%.3f' % min(idles))
print('BEST_TITLE=%.3f' % (min(titles) if titles else -1))
