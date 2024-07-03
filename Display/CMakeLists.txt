cmake_minimum_required(VERSION 3.8)

set(Display_SRCS)
set(Display_INCS)
set(Display_LIBS)
set(Display_LIBS_DIR)

set(SAMPLE_WITH_SDL ON)
set(SAMPLE_WITH_WAYLAND ON)

list(APPEND Display_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/AbstractDisplay.h
    ${CMAKE_CURRENT_SOURCE_DIR}/AbstractDisplay.cpp
)
include(SDL/SDL.cmake)
include(Wayland/Wayland.cmake)

list(APPEND Display_INCS
    ${CMAKE_SOURCE_DIR}/MMP-Core
    ${CMAKE_CURRENT_SOURCE_DIR}
)

add_library(Display STATIC ${Display_SRCS})
target_include_directories(Display PUBLIC ${Display_INCS})
target_link_libraries(Display PRIVATE Poco::Foundation Mmp::Common ${Display_LIBS}) 

if (SAMPLE_WITH_SDL)
    target_compile_definitions(Display PRIVATE SAMPLE_WITH_SDL)
endif()
if (SAMPLE_WITH_WAYLAND)
    target_compile_definitions(Display PRIVATE SAMPLE_WITH_WAYLAND)
endif()

list(LENGTH Display_LIBS_DIR Display_LIB_DIR_LEN)
if (NOT(${Display_LIB_DIR_LEN} EQUAL 0))
    link_directories(Display ${Display_LIBS_DIR})
endif()