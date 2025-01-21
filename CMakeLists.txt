cmake_minimum_required(VERSION 3.16)

project(AutoTLG VERSION 0.1 LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets Sql)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets Sql)

set(PROJECT_SOURCES
        main.cpp
        mainwindow.cpp
        mainwindow.h
        mainwindow.ui
)

if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
    qt_add_executable(AutoTLG
        MANUAL_FINALIZATION
        ${PROJECT_SOURCES}
        nonmodaldialogue.h nonmodaldialogue.cpp
        databasehandler.h databasehandler.cpp
        projectmanager.h projectmanager.cpp
        categorymanager.h categorymanager.cpp
        templatemanager.h templatemanager.cpp
        tablemanager.h tablemanager.cpp






    )
else()
    add_executable(AutoTLG
        ${PROJECT_SOURCES}
    )
endif()

target_link_libraries(AutoTLG PRIVATE Qt${QT_VERSION_MAJOR}::Widgets Qt${QT_VERSION_MAJOR}::Sql)

set_target_properties(AutoTLG PROPERTIES
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

include(GNUInstallDirs)
install(TARGETS AutoTLG
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(AutoTLG)
endif()
