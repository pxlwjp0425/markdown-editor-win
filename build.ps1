<#
    Markdown Editor - Windows 一键构建脚本（PowerShell）

    用法：
      .\build.ps1                 构建 Release 版并生成 dist/ 分发目录
      .\build.ps1 -Clean          清理后重新构建
      .\build.ps1 -Test           构建后运行自检（快捷键冲突 + 渲染断言）
#>
[CmdletBinding()]
param(
    [switch]$Clean,
    [switch]$Test)

$ErrorActionPreference = 'Stop'

# 子进程（Python 自检脚本）输出 UTF-8 中文，控制台也切到 UTF-8 避免乱码
try {
    [Console]::OutputEncoding = [System.Text.Encoding]::UTF8
    $OutputEncoding = [System.Text.Encoding]::UTF8
    $env:PYTHONIOENCODING = 'utf-8'
} catch { }

$Root     = $PSScriptRoot
$QtRoot   = 'D:\Qt\6.11.2\mingw_64'
$Mingw    = 'D:\Qt\Tools\mingw1310_64'
$Cmake    = 'D:\Qt\Tools\CMake_64\bin\cmake.exe'
$CTest    = 'D:\Qt\Tools\CMake_64\bin\ctest.exe'
$Ninja    = 'D:\Qt\Tools\Ninja\ninja.exe'
$DeployQt = 'D:\Qt\6.11.2\mingw_64\bin\windeployqt.exe'
$Python   = 'C:\Users\pxlwj\.workbuddy\binaries\python\versions\3.13.12\python.exe'

function Step($msg) {
    Write-Host ''
    Write-Host "==> $msg" -ForegroundColor Cyan
}

function Assert-Tool($path, $name) {
    if (-not (Test-Path $path)) {
        Write-Host "[错误] 未找到 $name : $path" -ForegroundColor Red
        exit 1
    }
}

Assert-Tool $Cmake    'cmake'
Assert-Tool $Ninja    'ninja'
Assert-Tool $DeployQt 'windeployqt'
Assert-Tool "$Mingw\bin\g++.exe" 'g++ (MinGW)'

# MinGW 的运行时与工具链必须在前置 PATH 中：
# g++ 调用 cc1plus 需要 libstdc++-6.dll / libwinpthread-1.dll，
# 链接阶段同样需要。缺失时 CMake 的编译器检测会「静默失败」——
# 报 "compiler is broken" 但不给出任何编译错误。
$env:PATH = "$Mingw\bin;$env:PATH"

if ($Clean) {
    Step '清理旧构建产物'
    foreach ($d in @("$Root\build", "$Root\dist")) {
        if (Test-Path $d) { Remove-Item -Recurse -Force $d }
    }
}

Step '生成图标资源'
$iconDir = "$Root\resources\icons"
if (-not (Test-Path $iconDir)) { New-Item -ItemType Directory -Path $iconDir -Force | Out-Null }
& $Python "$Root\tools\gen_icons.py" $iconDir
if ($LASTEXITCODE -ne 0) { Write-Host '[错误] 图标生成失败' -ForegroundColor Red; exit 1 }

Step '确保应用图标（app.ico）'
# app.ico 用于嵌入 exe（app.rc）。它只能由程序自己渲染 SVG 生成，
# 所以这里做个增量检查：SVG 比 ico 新、或 ico 不存在，就重新生成。
# 首次构建时还没有 exe，无法生成 —— 跳过并提示，第二次构建就好了。
$icoPath = Join-Path $Root 'resources\app.ico'
$svgList = @(
    (Join-Path $Root 'resources\icons\app-icon.svg'),
    (Join-Path $Root 'resources\icons\app-icon-small.svg')
)
$needIco = -not (Test-Path $icoPath)
if (-not $needIco) {
    $icoTime = (Get-Item $icoPath).LastWriteTime
    foreach ($s in $svgList) {
        if ((Test-Path $s) -and ((Get-Item $s).LastWriteTime -gt $icoTime)) { $needIco = $true }
    }
}
if ($needIco) {
    $genExe = Join-Path $Root 'dist\MarkdownEditor.exe'
    if (-not (Test-Path $genExe)) { $genExe = Join-Path $Root 'build\bin\MarkdownEditor.exe' }
    if (Test-Path $genExe) {
        & $genExe --export-icon $icoPath
        if (Test-Path "$icoPath.log") { Remove-Item "$icoPath.log" -Force -ErrorAction SilentlyContinue }
        if (Test-Path $icoPath) {
            Write-Host ("    已生成 app.ico ({0:N0} 字节)" -f (Get-Item $icoPath).Length) -ForegroundColor DarkGray
        }
    } else {
        Write-Host '    [跳过] 尚无 exe 可用于渲染图标，本次 exe 将使用默认图标；再构建一次即可' -ForegroundColor Yellow
    }
} else {
    Write-Host '    app.ico 已是最新' -ForegroundColor DarkGray
}

Step '配置 CMake'
if (-not (Test-Path "$Root\build")) { New-Item -ItemType Directory -Path "$Root\build" -Force | Out-Null }
& $Cmake -S $Root -B "$Root\build" -G Ninja `
    "-DCMAKE_MAKE_PROGRAM=$Ninja" `
    "-DCMAKE_CXX_COMPILER=$Mingw\bin\g++.exe" `
    -DCMAKE_BUILD_TYPE=Release `
    "-DCMAKE_PREFIX_PATH=$QtRoot"
if ($LASTEXITCODE -ne 0) { Write-Host '[错误] CMake 配置失败' -ForegroundColor Red; exit 1 }

Step '编译'
# 注意：Windows 文件系统时间戳精度较低，Ninja 可能漏判「同秒内修改」的文件，
# 导致改动没有真正编进二进制。强制 touch 源文件可避免这一类假象。
Get-ChildItem "$Root\src" -Include *.cpp, *.h -Recurse | ForEach-Object { $_.LastWriteTime = Get-Date }
& $Cmake --build "$Root\build"
if ($LASTEXITCODE -ne 0) { Write-Host '[错误] 编译失败' -ForegroundColor Red; exit 1 }

Step '打包分发目录'
if (-not (Test-Path "$Root\dist")) { New-Item -ItemType Directory -Path "$Root\dist" -Force | Out-Null }
& $DeployQt --release --no-translations --no-system-d3d-compiler `
    --no-opengl-sw --dir "$Root\dist" "$Root\build\bin\MarkdownEditor.exe"
Copy-Item "$Root\build\bin\MarkdownEditor.exe" "$Root\dist\MarkdownEditor.exe" -Force

if ($Test) {
    Step '自检：单元测试（Qt Test）'
    # 纯逻辑类的单元测试（DocModel / BlockEditor / InlineFormatter / SmartPunctuation）。
    # 这些是解析与格式化的正确性所在，脚本级断言覆盖不到它们。
    Push-Location "$Root\build"
    & $CTest --output-on-failure
    $ctestCode = $LASTEXITCODE
    Pop-Location
    if ($ctestCode -ne 0) { Write-Host '[错误] 单元测试未通过' -ForegroundColor Red; exit 1 }

    Step '自检：快捷键冲突扫描'
    & $Python "$Root\tools\check_shortcuts.py" "$Root\src\mainwindow.cpp"
    if ($LASTEXITCODE -ne 0) { Write-Host '[错误] 存在快捷键冲突' -ForegroundColor Red; exit 1 }

    if (-not (Test-Path "$Root\testdata")) { New-Item -ItemType Directory -Path "$Root\testdata" -Force | Out-Null }

    Step '自检：Markdown 渲染断言'
    & $Python "$Root\tools\selftest_render.py" "$Root\dist\MarkdownEditor.exe" "$Root\testdata"
    if ($LASTEXITCODE -ne 0) { Write-Host '[错误] 渲染自检未通过' -ForegroundColor Red; exit 1 }

    Step '自检：Typora 风格功能断言'
    & $Python "$Root\tools\selftest_typora.py" "$Root\dist\MarkdownEditor.exe" "$Root\testdata"
    if ($LASTEXITCODE -ne 0) { Write-Host '[错误] Typora 功能自检未通过' -ForegroundColor Red; exit 1 }

    Step '自检：主题系统断言'
    & $Python "$Root\tools\selftest_themes.py" "$Root\dist\MarkdownEditor.exe" "$Root\testdata"
    if ($LASTEXITCODE -ne 0) { Write-Host '[错误] 主题自检未通过' -ForegroundColor Red; exit 1 }

    Step '自检：单独预览模式断言'
    & $Python "$Root\tools\selftest_preview_only.py" "$Root\dist\MarkdownEditor.exe" "$Root\testdata"
    if ($LASTEXITCODE -ne 0) { Write-Host '[错误] 预览模式自检未通过' -ForegroundColor Red; exit 1 }

    Step '自检：Typora UI 规格断言'
    # 注意：本程序是 GUI 子系统程序，PowerShell 用 & 调用不会等待它结束，
    # 必须用 Start-Process -Wait，否则会在文件/输出还没生成时就往下走。
    & $Python "$Root\tools\selftest_typora_ui.py" "$Root\dist\MarkdownEditor.exe" "$Root\testdata"
    if ($LASTEXITCODE -ne 0) { Write-Host '[错误] Typora UI 自检未通过' -ForegroundColor Red; exit 1 }

    Step '自检：导出格式（容器与结构）'
    $exportDir = Join-Path $Root 'dist\out-test'
    if (Test-Path $exportDir) { Remove-Item $exportDir -Recurse -Force }
    New-Item -ItemType Directory -Path $exportDir -Force | Out-Null
    $sample = Join-Path $Root 'testdata\typora-features.md'
    $p = Start-Process -FilePath "$Root\dist\MarkdownEditor.exe" `
        -ArgumentList @('--self-test-export', $exportDir, $sample) `
        -Wait -PassThru -WindowStyle Hidden
    Write-Host ("    导出进程退出码 $($p.ExitCode)") -ForegroundColor DarkGray
    & $Python "$Root\tools\selftest_export.py" $exportDir
    if ($LASTEXITCODE -ne 0) { Write-Host '[错误] 导出格式自检未通过' -ForegroundColor Red; exit 1 }

    Step '自检：导出格式（深度校验）'
    & $Python "$Root\tools\selftest_export_deep.py" $exportDir $sample
    if ($LASTEXITCODE -ne 0) { Write-Host '[错误] 导出深度校验未通过' -ForegroundColor Red; exit 1 }
}

Write-Host ''
Write-Host '============================================' -ForegroundColor Green
Write-Host ' 构建完成！' -ForegroundColor Green
Write-Host " 可执行文件: $Root\dist\MarkdownEditor.exe" -ForegroundColor Green
Write-Host " 分发目录  : $Root\dist" -ForegroundColor Green
Write-Host '============================================' -ForegroundColor Green
