#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
测量 Typora 打开 Markdown 文件的耗时（外部黑盒测量）。

由于 Typora 是 Electron 应用、没有内部打点，只能从外部测：
  t_window   主窗口出现
  t_title    窗口标题变成目标文件名（说明文件已读入）
  t_idle     CPU 时间停止增长（说明渲染完成，可交互）

用法：bench_typora.py <exe> <md文件> [重复次数]
"""
import ctypes
import ctypes.wintypes as wt
import os
import subprocess
import sys
import time

EXE = sys.argv[1]
MD = sys.argv[2]
ROUNDS = int(sys.argv[3]) if len(sys.argv) > 3 else 1

user32 = ctypes.windll.user32
kernel32 = ctypes.windll.kernel32
psapi = ctypes.windll.psapi

user32.SetProcessDPIAware()

WNDENUMPROC = ctypes.WINFUNCTYPE(wt.BOOL, wt.HWND, wt.LPARAM)


def find_windows_for_pid(pid):
    """找出属于指定 pid 的所有可见顶层窗口"""
    found = []

    def cb(hwnd, lparam):
        wpid = wt.DWORD()
        user32.GetWindowThreadProcessId(hwnd, ctypes.byref(wpid))
        if wpid.value == pid and user32.IsWindowVisible(hwnd):
            length = user32.GetWindowTextLengthW(hwnd)
            if length > 0:
                buf = ctypes.create_unicode_buffer(length + 1)
                user32.GetWindowTextW(hwnd, buf, length + 1)
                found.append((hwnd, buf.value))
        return True

    user32.EnumWindows(WNDENUMPROC(cb), 0)
    return found


def proc_cpu_seconds(pid):
    """取进程累计 CPU 时间（秒）"""
    h = kernel32.OpenProcess(0x0400 | 0x0010, False, pid)   # QUERY_INFORMATION|VM_READ
    if not h:
        return None
    try:
        create = wt.FILETIME()
        exit_ = wt.FILETIME()
        kern = wt.FILETIME()
        user = wt.FILETIME()
        if not kernel32.GetProcessTimes(h, ctypes.byref(create), ctypes.byref(exit_),
                                        ctypes.byref(kern), ctypes.byref(user)):
            return None
        to_int = lambda ft: (ft.dwHighDateTime << 32) | ft.dwLowDateTime
        return (to_int(kern) + to_int(user)) / 1e7
    finally:
        kernel32.CloseHandle(h)


def kill_tree(proc):
    subprocess.run(['taskkill', '/F', '/T', '/PID', str(proc.pid)],
                   capture_output=True)


results = []
basename = os.path.basename(MD)

for r in range(ROUNDS):
    # 确保干净启动
    subprocess.run(['taskkill', '/F', '/IM', 'Typora.exe'], capture_output=True)
    time.sleep(1.5)

    t0 = time.perf_counter()
    proc = subprocess.Popen([EXE, MD])

    t_window = None
    t_title = None
    deadline = t0 + 120
    idle_since = None
    last_cpu = 0.0

    while time.perf_counter() < deadline:
        now = time.perf_counter()

        if t_title is None:
            wins = find_windows_for_pid(proc.pid)
            if wins and t_window is None:
                t_window = now - t0
            for hwnd, title in wins:
                if basename in title:
                    t_title = now - t0
                    break

        # 标题出现后开始检测 CPU 空闲
        if t_title is not None:
            cpu = proc_cpu_seconds(proc.pid)
            if cpu is not None:
                if cpu - last_cpu < 0.02:      # 20ms 内 CPU 几乎没动
                    if idle_since is None:
                        idle_since = now
                    elif now - idle_since > 0.6:   # 持续 600ms 空闲
                        break
                else:
                    idle_since = None
                last_cpu = cpu
        time.sleep(0.03)

    t_end = time.perf_counter() - t0
    results.append({
        'window': t_window,
        'title': t_title,
        'idle': t_end,
    })
    print('  round %d: window=%s title=%s idle=%.2fs' % (
        r + 1,
        ('%.2fs' % t_window) if t_window else 'N/A',
        ('%.2fs' % t_title) if t_title else 'N/A',
        t_end))

    kill_tree(proc)
    time.sleep(0.8)

print()
print('=== Typora 汇总 (%d 轮) ===' % ROUNDS)
for key, label in [('window', '窗口出现'), ('title', '标题就绪'), ('idle', '渲染完成')]:
    vals = [r[key] for r in results if r[key]]
    if vals:
        print('  %-8s min=%.2fs  avg=%.2fs  max=%.2fs' % (
            label, min(vals), sum(vals) / len(vals), max(vals)))

# 机器可读输出
best = min((r['idle'] for r in results), default=0)
print('\nBEST_IDLE_SEC=%.3f' % best)
