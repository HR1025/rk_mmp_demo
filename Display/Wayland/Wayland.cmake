cmake_minimum_required(VERSION 3.8)

set(WAYLAND_SRCS)
set(WAYLAND_INCS)

include_directories(${WAYLAND2_INCLUDE_DIRS})

if (NOT WAYLAND_FOUND)
    find_package(PkgConfig)
    pkg_check_modules(WAYLAND wayland-client)
endif()

message("-- Found WAYLAND")
message("  -- WAYLAND INCS : ${WAYLAND_INCLUDE_DIRS}")
message("  -- WAYLAND LIB DIR : ${WAYLAND_LIBRARY_DIRS}")
message("  -- WAYLAND LIBS : ${WAYLAND_LIBRARIES}")

list(APPEND WAYLAND_INCS ${WAYLAND_INCLUDE_DIRS})

list(APPEND WAYLAND_INCS
    ${MMP_PRO_TOP_DIR}
    ${MMP_CORE_PRO_TOP_DIR}
)
list(APPEND WAYLAND_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/Display/Wayland/DisplayWayland.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Display/Wayland/DisplayWayland.cpp
)

add_library(Sample_WAYLAND OBJECT ${WAYLAND_SRCS})
add_library(Sample::WAYLAND ALIAS Sample_WAYLAND)
target_include_directories(Sample_WAYLAND PUBLIC ${WAYLAND_INCS})

list(APPEND Common_LIBS
    Poco::Foundation
    Sample::WAYLAND
    ${WAYLAND_LIBRARIES}
)
list(APPEND Common_LIBS_DIR
    ${WAYLAND_LIBRARY_DIRS}
)