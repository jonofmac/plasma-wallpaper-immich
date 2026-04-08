# SPDX-License-Identifier: BSD-2-Clause
# Install wallpaper KPackage under share/plasma/wallpapers/<component>
macro(plasma_install_wallpaper_package dir component)
    install(DIRECTORY "${dir}/" DESTINATION "${KDE_INSTALL_DATADIR}/plasma/wallpapers/${component}"
        PATTERN .svn EXCLUDE
        PATTERN "*.qmlc" EXCLUDE
        PATTERN CMakeLists.txt EXCLUDE)
endmacro()
