QT       += core gui network webenginewidgets

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++11

# The following define makes your compiler emit warnings if you use
# any Qt feature that has been marked deprecated (the exact warnings
# depend on your compiler). You should consult the documentation of
# the deprecated API in order to know how to port your code away from it.
DEFINES += QT_DEPRECATED_WARNINGS

# You can also make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
# You can also select to disable deprecated APIs only up to a certain version.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
    main.cpp \
    widget.cpp \
    compasswidget.cpp \
    mapdialog.cpp \
    minimapdialog.cpp \
    rotationdial.cpp \
    px4controller.cpp \
    radarwidget.cpp

HEADERS += \
    widget.h \
    compasswidget.h \
    mapdialog.h \
    minimapdialog.h \
    rotationdial.h \
    px4controller.h \
    radarwidget.h

FORMS += \
    widget.ui

RESOURCES += \
    resources.qrc

# =============================================================================
# MAVSDK-C++ v3 集成（PX4 仿真通信）
# =============================================================================
# MAVSDK 头文件与库通过官方 deb 包安装：
#   libmavsdk-dev_3.17.2_ubuntu20.04_amd64.deb
# 安装后头文件位于 /usr/include/mavsdk，库位于 /usr/lib
unix:!macx {
    # 启用 C++17（MAVSDK v3 要求）
    CONFIG += c++17

    # MAVSDK 头文件路径 + C++ 标准库路径
    # 注意：C++ 标准库路径必须在 Qt 路径之前，以确保 #include_next 正确解析
    INCLUDEPATH += /usr/include/c++/9 \
                   /usr/include/x86_64-linux-gnu/c++/9 \
                   /usr/include/c++/9/backward \
                   /usr/lib/gcc/x86_64-linux-gnu/9/include \
                   /usr/include/x86_64-linux-gnu \
                   /usr/include \
                   /usr/include/mavsdk

    # 链接 MAVSDK 共享库
    # 直接指定完整路径，避免需要 libmavsdk.so 开发链接
    LIBS += /usr/lib/libmavsdk.so.3.17.1
    LIBS += -lpthread

    # 运行时 RPATH，确保可找到 libmavsdk.so.3
    QMAKE_LFLAGS += -Wl,-rpath,/usr/lib

    # 禁用 moc 调试信息中的交叉编译路径（防止 MinGW 路径污染）
    QMAKE_MOC += --no-include-path
}

# =============================================================================
# OpenCV 链接（用于 USB 摄像头视频采集，显示到视频显示窗口）
# 通过 pkg-config 自动获取头文件路径与库
# =============================================================================
unix:!macx {
    CONFIG += link_pkgconfig
    PKGCONFIG += opencv4
}

# Platform-specific configuration
# Windows platform: enable PowerShell-based GPS positioning
win32 {
    DEFINES += Q_OS_WIN
}

# Linux/Unix platform: use IP positioning instead of PowerShell
unix:!macx {
    DEFINES += Q_OS_LINUX
}

# macOS platform
macx {
    DEFINES += Q_OS_MAC
}

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

# PowerShell定位脚本仅在Windows平台使用
# Linux平台直接使用IP定位

DISTFILES += \
    amap.html \
    map.qml \
    run_simulation.sh
