QT += core gui widgets printsupport svg

CONFIG += c++17

TARGET = MarkdownEditor
TEMPLATE = app

# Windows GUI 程序，不附带控制台窗口
win32: CONFIG += windows

# MinGW 下需要 unicode 入口点
win32-g++: QMAKE_LFLAGS += -municode

DEFINES += QT_DEPRECATED_WARNINGS
DEFINES += QT_RESTRICTED_CAST_FROM_ASCII

RC_FILE = app.rc

SOURCES += \
    src/main.cpp \
    src/mainwindow.cpp \
    src/mainwindow_export.cpp \
    src/mainwindow_format.cpp \
    src/mainwindow_selftest.cpp \
    src/mainwindow_devtools.cpp \
    src/markdownhighlighter.cpp \
    src/filemanager.cpp \
    src/searchreplace.cpp \
    src/theme.cpp \
    src/outlinepanel.cpp \
    src/smartpunctuation.cpp \
    src/inlineformatter.cpp \
    src/blockeditor.cpp \
    src/docmodel.cpp \
    src/htmlrenderer.cpp \
    src/exporter.cpp \
    src/exporter_office.cpp \
    src/zipwriter.cpp

HEADERS += \
    src/mainwindow.h \
    src/markdownhighlighter.h \
    src/filemanager.h \
    src/searchreplace.h \
    src/theme.h \
    src/outlinepanel.h \
    src/smartpunctuation.h \
    src/inlineformatter.h \
    src/blockeditor.h \
    src/docmodel.h \
    src/htmlrenderer.h \
    src/exporter.h \
    src/zipwriter.h

# 内置 SVG 图标（由 tools/gen_icons.py 生成）
RESOURCES += \
    resources/icons.qrc

# 无 .ui 文件，界面全部代码创建
qmake: FORCE
