cmake_minimum_required(VERSION 3.9)
project(dabc-ncurses)
find_package(DABC)
include(${DABC_USE_FILE})

find_package(PkgConfig)
if (PkgConfig_FOUND)
    pkg_check_modules(ncurses ncurses menu IMPORTED_TARGET)

    if (ncurses_FOUND)
        DABC_EXECUTABLE(dabc_shell
                        SOURCES dabc_shell.cxx
                        LIBRARIES ${DabcBase_LIBRARY} PkgConfig::ncurses
                        CHECKSTD)
    endif()
endif()
