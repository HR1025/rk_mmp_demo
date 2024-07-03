cmake_minimum_required(VERSION 3.8)

if (NOT WAYLAND_FOUND)
    find_package(PkgConfig)
    pkg_check_modules(WAYLAND wayland-client)
endif()

if (WAYLAND_FOUND)
    message("-- Found WAYLAND")
    message("  -- WAYLAND INCS : ${WAYLAND_INCLUDE_DIRS}")
    message("  -- WAYLAND LIB DIR : ${WAYLAND_LIBRARY_DIRS}")
    message("  -- WAYLAND LIBS : ${WAYLAND_LIBRARIES}")
    list(APPEND Display_INCS ${WAYLAND_INCLUDE_DIRS})
    list(APPEND Display_SRCS
        ${CMAKE_CURRENT_SOURCE_DIR}/Wayland/DisplayWayland.h
        ${CMAKE_CURRENT_SOURCE_DIR}/Wayland/DisplayWayland.cpp
    )

else()
    set(SAMPLE_WITH_WAYLAND OFF)
endif()