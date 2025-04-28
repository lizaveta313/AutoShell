cmake_minimum_required(VERSION 3.16)

project(AutoTLG VERSION 0.1 LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Sql Network)

set(PROJECT_SOURCES
        main.cpp
        mainwindow.cpp
        mainwindow.h
        mainwindow.ui
)

qt_add_executable(AutoTLG
    MANUAL_FINALIZATION
    ${PROJECT_SOURCES}
    nonmodaldialogue.h nonmodaldialogue.cpp
    databasehandler.h databasehandler.cpp
    projectmanager.h projectmanager.cpp
    categorymanager.h categorymanager.cpp
    templatemanager.h templatemanager.cpp
    tablemanager.h tablemanager.cpp
    mytreewidget.h mytreewidget.cpp
    richtextdelegate.h richtextdelegate.cpp
    resources.qrc
    formattoolbar.h formattoolbar.cpp
    projectpanel.h projectpanel.cpp
    treecategorypanel.h treecategorypanel.cpp
    templatepanel.h templatepanel.cpp
    exportprojectasxml.h exportprojectasxml.cpp

)

target_link_libraries(AutoTLG PRIVATE
    Qt6::Widgets
    Qt6::Core
    Qt6::Gui
    Qt6::Sql
    Qt6::Network
)
set_target_properties(AutoTLG PROPERTIES
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(AutoTLG)
endif()

include(GNUInstallDirs)
install(TARGETS AutoTLG
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(AutoTLG)
endif()
