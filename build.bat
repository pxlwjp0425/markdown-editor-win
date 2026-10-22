@echo off
REM ============================================================================
REM  Markdown Editor - Windows 一键构建脚本
REM
REM  用法：
REM    build.bat              构建 Release 版并生成 dist/ 分发目录
REM    build.bat clean        清理后重新构建
REM    build.bat test         构建后运行自检
REM ============================================================================
setlocal enabledelayedexpansion

set "ROOT=%~dp0"
set "QT_ROOT=D:\Qt\6.11.2\mingw_64"
set "MINGW=D:\Qt\Tools\mingw1310_64"
set "CMAKE=D:\Qt\Tools\CMake_64\bin\cmake.exe"
set "NINJA=D:\Qt\Tools\Ninja\ninja.exe"
set "WINDEPLOY=D:\Qt\6.11.2\mingw_64\bin\windeployqt.exe"
set "PY=C:\Users\pxlwj\.workbuddy\binaries\python\versions\3.13.12\python.exe"

set "ACTION=%~1"
if "%ACTION%"=="clean" (
    echo [1/5] 清理旧构建产物...
    if exist "%ROOT%build" rmdir /s /q "%ROOT%build"
    if exist "%ROOT%dist" rmdir /s /q "%ROOT%dist"
)

echo [1/5] 生成图标资源...
if not exist "%ROOT%resources\icons" mkdir "%ROOT%resources\icons"
"%PY%" "%ROOT%tools\gen_icons.py" "%ROOT%resources\icons"
if errorlevel 1 goto :fail

echo [2/5] 配置 CMake...
if not exist "%ROOT%build" mkdir "%ROOT%build"
"%CMAKE%" -S "%ROOT%" -B "%ROOT%build" -G Ninja ^
    -DCMAKE_MAKE_PROGRAM="%NINJA%" ^
    -DCMAKE_CXX_COMPILER="%MINGW%\bin\g++.exe" ^
    -DCMAKE_BUILD_TYPE=Release ^
    -DCMAKE_PREFIX_PATH="%QT_ROOT%"
if errorlevel 1 goto :fail

echo [3/5] 编译...
"%CMAKE%" --build "%ROOT%build"
if errorlevel 1 goto :fail

echo [4/5] 打包分发目录...
if not exist "%ROOT%dist" mkdir "%ROOT%dist"
"%WINDEPLOY%" --release --no-translations --no-system-d3d-compiler ^
    --no-opengl-sw --dir "%ROOT%dist" "%ROOT%build\bin\MarkdownEditor.exe"
copy /Y "%ROOT%build\bin\MarkdownEditor.exe" "%ROOT%dist\MarkdownEditor.exe" >nul

if /i "%ACTION%"=="test" goto :test

echo.
echo ============================================
echo  构建完成！
echo  可执行文件: %ROOT%dist\MarkdownEditor.exe
echo  分发目录  : %ROOT%dist\
echo ============================================
goto :eof

:test
echo [5/5] 运行自检...
"%PY%" "%ROOT%tools\check_shortcuts.py" "%ROOT%src\mainwindow.cpp"
if errorlevel 1 goto :fail
"%PY%" "%ROOT%tools\selftest_render.py" "%ROOT%dist\MarkdownEditor.exe" "%ROOT%testdata"
if errorlevel 1 goto :fail
echo.
echo 全部自检通过。
goto :eof

:fail
echo.
echo [错误] 构建失败，请检查上方输出。
exit /b 1
