cmake_minimum_required(VERSION 3.15)

project(www VERSION 0.1 LANGUAGES CXX)

# ==============================
# MSVC UTF-8
# ==============================
if(MSVC)
    add_compile_options(/utf-8)
    # 确保 __cplusplus 宏正确报告 C++ 版本（C++/WinRT 需要）
    add_compile_options(/Zc:__cplusplus)
endif()

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ==============================
# Qt（移除 WebEngine，新增 Concurrent）
# ==============================
find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS
    Widgets
    Network
    Xml
    Gui
    Concurrent
    PrintSupport
    LinguistTools
)

find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS
    Widgets
    Network
    Xml
    Gui
    Concurrent
    PrintSupport
    LinguistTools
)

set(TS_FILES www_zh_CN.ts)

# ==============================
# 源文件
# ==============================
set(PROJECT_SOURCES
    main.cpp
    mainwindow.cpp
    mainwindow.h
    mainwindow.ui

    linklist.h
    linklist.cpp
    photoalbum.h
    photoalbum.cpp
    photoalbum.ui
    bin.h
    bin.cpp
    bin.ui
    tcp.h tcp.cpp
    printrecord.h printrecord.cpp printrecord.ui
    print.h print.cpp print.ui
    printworkflow.h printworkflow.cpp
    convertfile.h convertfile.cpp
    pdfrenderer.h pdfrenderer.cpp
    inkjet.h inkjet.cpp
    ${TS_FILES}
)

# ==============================
# 可执行程序
# ==============================
if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
    qt_add_executable(www
        MANUAL_FINALIZATION
        ${PROJECT_SOURCES}
    )
else()
    add_executable(www
        ${PROJECT_SOURCES}
        image.qrc
    )
endif()

# ==============================
# 链接库
# ==============================
target_link_libraries(www PRIVATE
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::Network
    Qt${QT_VERSION_MAJOR}::Xml
    Qt${QT_VERSION_MAJOR}::Gui
    Qt${QT_VERSION_MAJOR}::Concurrent
    Qt${QT_VERSION_MAJOR}::PrintSupport
)

# Windows.Data.Pdf 等 WinRT API（Windows 10 内置，无需安装任何外部库）
if(WIN32)
    target_link_libraries(www PRIVATE windowsapp)
endif()

# ==============================
# Windows 程序（不显示控制台）
# ==============================
set_target_properties(www PROPERTIES
    WIN32_EXECUTABLE TRUE
)

# ==============================
# Qt6 finalize
# ==============================
if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(www)
endif()
