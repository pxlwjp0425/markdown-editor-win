cmake_minimum_required(VERSION 3.16)
project(MarkdownEditor LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ---------------------------------------------------------------------------
# Windows 移植说明
#
# 与原版 CMakeLists.txt 的差异：
#  1. find_package(Qt6.5 ...) 改为 find_package(Qt6 ...) 且不锁死小版本。
#     原写法在只装了 Qt 6.11.2 的机器上会直接失败：
#     "Could not find a package configuration file provided by "Qt6.5""
#  2. 新增 Svg 模块 —— 图标是内置 SVG，需要 Qt6::Svg 提供的 QSvgRenderer。
#  3. WIN32_EXECUTABLE：Windows GUI 程序，不弹控制台窗口。
#  4. 资源文件改为自动生成（见 tools/gen_icons.py），图标随程序打包。
#  5. 输出目录统一为 bin/，便于 windeployqt 收集依赖。
# ---------------------------------------------------------------------------

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# Qt 6 主版本固定为 6，小版本不锁死（兼容 6.5 ~ 6.11+）
find_package(Qt6 REQUIRED COMPONENTS
    Core
    Gui
    Widgets
    PrintSupport
    Svg
)

qt_standard_project_setup()

set(SOURCES
    src/main.cpp
    src/mainwindow.cpp
    src/mainwindow_export.cpp
    src/mainwindow_format.cpp
    src/mainwindow_selftest.cpp
    src/mainwindow_devtools.cpp
    src/markdownhighlighter.cpp
    src/filemanager.cpp
    src/searchreplace.cpp
    src/theme.cpp
    src/outlinepanel.cpp
    src/exporter.cpp
    src/exporter_office.cpp
    src/zipwriter.cpp
    src/mergedialog.cpp
)

set(HEADERS
    src/mainwindow.h
    src/markdownhighlighter.h
    src/filemanager.h
    src/searchreplace.h
    src/theme.h
    src/outlinepanel.h
    src/exporter.h
    src/zipwriter.h
    src/mergedialog.h
)

# ---------------------------------------------------------------------------
# 核心逻辑库
#
# docmodel / htmlrenderer / blockeditor / inlineformatter / smartpunctuation
# 只依赖 QtCore，不碰任何 GUI 部件。抽成独立静态库有两个好处：
#   1. 单元测试可以直接链接它，不需要拉起整个 Widgets 栈
#   2. 这些是纯函数类，改动频繁，单独编译比跟着整个程序编译快
# ---------------------------------------------------------------------------
add_library(mdcore STATIC
    src/docmodel.cpp
    src/htmlrenderer.cpp
    src/blockeditor.cpp
    src/inlineformatter.cpp
    src/smartpunctuation.cpp
    src/mergecore.cpp
    src/pdfoutline.cpp
    src/docmodel.h
    src/htmlrenderer.h
    src/blockeditor.h
    src/inlineformatter.h
    src/smartpunctuation.h
    src/mergecore.h
    src/pdfoutline.h
)
target_include_directories(mdcore PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(mdcore PUBLIC Qt6::Core)

# 图标资源（由 tools/gen_icons.py 生成）
set(RESOURCES
    resources/icons.qrc
)

# 图标生成作为显式构建步骤：确保 qrc 存在且内容最新，
# 避免 Ninja 并行构建时 qrc_icons.cpp 的依赖扫描与 RCC 竞争
find_package(Python3 COMPONENTS Interpreter QUIET)
if(Python3_FOUND)
    add_custom_command(
        OUTPUT ${CMAKE_CURRENT_SOURCE_DIR}/resources/icons.qrc
        COMMAND ${Python3_EXECUTABLE}
                ${CMAKE_CURRENT_SOURCE_DIR}/tools/gen_icons.py
                ${CMAKE_CURRENT_SOURCE_DIR}/resources/icons
        DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/tools/gen_icons.py
        COMMENT "生成 SVG 图标资源"
        VERBATIM
    )
    add_custom_target(icons ALL
        DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/resources/icons.qrc
    )
endif()

add_executable(${PROJECT_NAME} WIN32
    ${SOURCES}
    ${HEADERS}
    ${RESOURCES}
)

# 目标依赖必须在 add_executable 之后声明
if(TARGET icons)
    add_dependencies(${PROJECT_NAME} icons)
endif()

target_link_libraries(${PROJECT_NAME} PRIVATE
    mdcore
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::PrintSupport
    Qt6::Svg
)

target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

# ---------------------------------------------------------------------------
# 编译警告
#
# 之前编译脚本里带了 -Wall 但没写进 CMakeLists，等于只在命令行生效 ——
# 谁用 IDE 或者别的工具链构建就悄悄没有了。
# 现在固化到工程配置里：GCC/Clang 用 -Wall -Wextra，MSVC 用 /W4。
# ---------------------------------------------------------------------------
if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE /W4)
    target_compile_options(mdcore PRIVATE /W4)
else()
    target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra)
    target_compile_options(mdcore PRIVATE -Wall -Wextra)
endif()

# Windows 下 UTF-8 源码字符集，避免 MSVC 对中文源码的编码歧义
if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE /utf-8)
    target_compile_definitions(${PROJECT_NAME} PRIVATE
        UNICODE _UNICODE
        WIN32_LEAN_AND_MEAN
        NOMINMAX
    )
endif()

# MinGW：链接 Win32 库需要显式指定入口点为 WinMain
if(MINGW)
    target_link_options(${PROJECT_NAME} PRIVATE -municode)
endif()

# ---------------------------------------------------------------------------
# Windows 可执行文件图标
#
# 需要 app.rc 把 resources/app.ico 嵌进 exe，否则资源管理器/任务栏
# 显示的是系统默认图标。app.ico 由程序自身的 --export-icon 生成：
#     dist/MarkdownEditor.exe --export-icon resources/app.ico
# （生成 ICO 需要 SVG 渲染能力，所以只能由程序自己做，不能在
#   tools/gen_icons.py 里纯文本生成）
# ---------------------------------------------------------------------------
if(WIN32)
    set(APP_ICON_FILE ${CMAKE_CURRENT_SOURCE_DIR}/resources/app.ico)
    if(EXISTS ${APP_ICON_FILE})
        enable_language(RC)
        target_sources(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/app.rc)
        message(STATUS "应用图标: ${APP_ICON_FILE}")
    else()
        message(WARNING
            "未找到 resources/app.ico，exe 将使用系统默认图标。\n"
            "先生成一次：dist/MarkdownEditor.exe --export-icon resources/app.ico")
    endif()
endif()

set_target_properties(${PROJECT_NAME} PROPERTIES
    OUTPUT_NAME "MarkdownEditor"
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

# MinGW 构建时把 Qt 运行库一起带出（windeployqt 负责最终打包）
install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION .)

# ---------------------------------------------------------------------------
# 单元测试
#
# 现有自检全是「跑进程 + 解析输出」的脚本级断言，覆盖不到纯逻辑类。
# DocModel / BlockEditor / InlineFormatter / SmartPunctuation 这四个
# 是纯函数类，正是最需要单测的地方 —— 它们决定了解析与格式化的正确性。
# ---------------------------------------------------------------------------
enable_testing()
add_subdirectory(tests)
