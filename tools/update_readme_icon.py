#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""在 README 加「应用图标」章节。"""
import io

p = 'README.md'
s = io.open(p, encoding='utf-8').read()

SECTION = '''## 十、应用图标

原先用的是界面图标集里那个 24×24 的线性轮廓 `app.svg`（`stroke="currentColor"`，
本来给菜单和工具栏用的），拿来当应用图标太单薄；而且 **exe 本身没有嵌图标** ——
资源管理器、任务栏、Alt+Tab 里显示的都是系统默认图标。

### 10.1 设计

符号选了 **M↓** —— Markdown 的通行标识，识别度最高。配色用蓝色渐变（技术工具的常见取向），
圆角矩形底版，白色前景。

设计上最容易踩的坑是**小尺寸**：完整版 `M↓` 在 16/24/32 像素下，箭头和 M 会糊成一团白点。
所以做了两份 SVG：

| 文件 | 用途 | 内容 |
|---|---|---|
| `resources/icons/app-icon.svg` | 48 / 64 / 128 / 256 | 完整 `M↓`，带渐变高光 |
| `resources/icons/app-icon-small.svg` | 16 / 20 / 24 / 32 | 只有粗体 `M`，去掉箭头 |

两者视觉语言一致（同色底、同字重），只是小尺寸做了减法。
实测 16px 下 `M` 依然清晰可辨。

### 10.2 生成 ICO

Windows 的 exe 图标需要 **16/24/32/48/64/128/256 多档**，否则任务栏和大图标视图会模糊。
`QImage::save("x.ico")` 只能写单一尺寸，所以自己拼 ICO：

- ICO 结构很简单：`ICONDIR`(6字节) + `ICONDIRENTRY`×N(16字节) + 各图数据
- Vista 以后允许每个条目直接嵌 PNG，不用转 BMP
- `width/height` 字段只有 1 字节，**0 表示 256**

实现放在程序里（`--export-icon`），因为渲染 SVG 需要 `QSvgRenderer`：

```powershell
dist/MarkdownEditor.exe --export-icon resources/app.ico
```

产物 7 档、30 KB，全部为 PNG 嵌入。

### 10.3 嵌入 exe

`app.rc` 只有一行（`IDI_ICON1 ICON "resources/app.ico"`），CMake 里按需挂上：

```cmake
if(WIN32)
    if(EXISTS resources/app.ico)
        enable_language(RC)
        target_sources(${PROJECT_NAME} PRIVATE app.rc)
    else()
        message(WARNING "未找到 app.ico，exe 将使用系统默认图标")
    endif()
endif()
```

**有个先有鸡还是先有蛋的问题**：生成 ICO 需要 exe，编译 exe 需要 ICO。
处理办法是 `app.ico` 作为产物提交到仓库，构建时直接用；
`build.ps1` 里加了一步增量检查 —— 如果 SVG 比 ICO 新就重新生成，
首次构建（还没有 exe）则跳过并提示，第二次构建自动补上。

### 10.4 窗口图标

`QIcon("xx.svg")` 只会拿一份图去缩放，小尺寸同样会糊。
所以用 `addFile` 按尺寸分别绑定（`src/appicon.h` 里的 `makeAppIcon()`）：

```cpp
icon.addFile(":/icons/app-icon-small.svg", QSize(16, 16));   // 小尺寸用简化版
icon.addFile(":/icons/app-icon.svg",       QSize(48, 48));   // 大尺寸用完整版
```

### 10.5 验证

| 项 | 结果 |
|---|---|
| ICO 结构 | 7 档（16/24/32/48/64/128/256），全部 PNG 嵌入，30 KB |
| exe 图标 | `ExtractIconExW` 能取出图标组（`tools/check_exe_icon.py`） |
| 窗口标题栏 | 16 px 下 `M↓` 清晰可辨 |
| 各尺寸可辨性 | 16/24/32 只有 M；48 以上出现箭头（见 `shots/icon-sizes.png`） |

'''

marker = '## 十、验证结果'
assert marker in s
s = s.replace(marker, SECTION + '## 十一、验证结果', 1)
s = s.replace('## 十一、说明', '## 十二、说明', 1)

io.open(p, 'w', encoding='utf-8', newline='\n').write(s)
print('README 已加「十、应用图标」')
