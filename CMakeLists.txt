# SPDX-License-Identifier: GPL-2.0-or-later
cmake_minimum_required(VERSION 3.22)

project(plasma-wallpaper-immich VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(GNUInstallDirs)
if(NOT KDE_INSTALL_DATADIR)
    set(KDE_INSTALL_DATADIR "${CMAKE_INSTALL_DATADIR}")
endif()
if(NOT KDE_INSTALL_METAINFODIR)
    set(KDE_INSTALL_METAINFODIR "${CMAKE_INSTALL_DATADIR}/metainfo")
endif()
if(NOT KDE_INSTALL_QMLDIR)
    set(KDE_INSTALL_QMLDIR "${CMAKE_INSTALL_FULL_LIBDIR}/qt6/qml")
endif()

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Network Qml Quick)

if (COMMAND qt_standard_project_setup)
    qt_standard_project_setup(REQUIRES 6.5)
endif()

set(QT_QML_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/org/kde/plasma/wallpapers/immich")

qt_add_qml_module(plasma_wallpaper_immichplugin
    URI org.kde.plasma.wallpapers.immich
    VERSION 1.0
    PLUGIN_TARGET plasma_wallpaper_immichplugin
    SOURCES
        immichbackend.cpp
        immichbackend.h
)

set_target_properties(plasma_wallpaper_immichplugin PROPERTIES
    OUTPUT_NAME plasma_wallpaper_immichplugin
)

target_link_libraries(plasma_wallpaper_immichplugin PRIVATE
    Qt6::Core
    Qt6::Network
    Qt6::Qml
    Qt6::Quick
)

target_include_directories(plasma_wallpaper_immichplugin PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

install(TARGETS plasma_wallpaper_immichplugin
    LIBRARY DESTINATION ${KDE_INSTALL_QMLDIR}/org/kde/plasma/wallpapers/immich
    RUNTIME DESTINATION ${KDE_INSTALL_QMLDIR}/org/kde/plasma/wallpapers/immich
)

install(FILES
    "${QT_QML_OUTPUT_DIRECTORY}/org/kde/plasma/wallpapers/immich/qmldir"
    "${QT_QML_OUTPUT_DIRECTORY}/org/kde/plasma/wallpapers/immich/plasma_wallpaper_immichplugin.qmltypes"
    DESTINATION ${KDE_INSTALL_QMLDIR}/org/kde/plasma/wallpapers/immich
)

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/PlasmaInstallPackage.cmake")
plasma_install_wallpaper_package(package org.kde.plasma.wallpapers.immich)
