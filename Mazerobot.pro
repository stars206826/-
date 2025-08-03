# 迷宫生成与路径搜索项目
QT += core gui widgets

# C++ 标准配置
CONFIG += c++17

# Qt 版本配置
QT_MAJOR_VERSION = 6
QT_MINOR_VERSION = 6
QT_PATCH_VERSION = 3

# 源文件
SOURCES += \
    main.cpp \
    mainwindow.cpp

# 头文件
HEADERS += \
    mainwindow.h

# UI 文件
FORMS += \
    mainwindow.ui

# 资源文件（如果有）
# RESOURCES += resources.qrc

# 应用程序信息
TARGET = Mazerobot
TEMPLATE = app

# 编译选项
win32 {
    # MSVC编译器选项
    QMAKE_CXXFLAGS += /W3 /wd4100 /wd4189 /wd4996 /wd4456 /wd4457 /wd4458 /wd4577 /wd4467
} else {
    # GCC/Clang编译器选项
    QMAKE_CXXFLAGS += -Wall -Wextra -Wno-unused-parameter
}

# 调试信息配置
CONFIG(debug, debug|release) {
    CONFIG += debug_info
} else {
    CONFIG += release
}

# 安装配置
unix:!android {
    target.path = /usr/local/bin
    INSTALLS += target
}

win32 {
    target.path = $$[QT_INSTALL_EXAMPLES]/$${TARGET}
    INSTALLS += target
}

# 解决 Qt 6 兼容性问题
QT_VERSION = $$QT_MAJOR_VERSION.$${QT_MINOR_VERSION}
greaterThan(QT_VERSION, 5.15): {
    # Qt 6 兼容处理
    QMAKE_CXXFLAGS += -DQT_NO_FOREACH
    # 如果需要使用 QTimeLine，添加以下定义
    # QMAKE_CXXFLAGS += -DQT_DEPRECATED_WARNINGS
}

RESOURCES += \
    resources.qrc
