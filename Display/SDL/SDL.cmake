cmake_minimum_required(VERSION 3.8)

if (NOT SDL2_FOUND)
    find_package(SDL2 CONFIG)
endif()

if (SDL2_FOUND)
    message("-- Found SDL2")
    message("  -- SDL INCS : ${SDL2_INCLUDE_DIRS}")
    message("  -- SDL LIB DIR : ${SDL2_LIBRARY_DIRS}")
    message("  -- SDL LIBS : ${SDL2_LIBRARIES}")
    list(APPEND Display_INCS ${SDL2_INCLUDE_DIRS})
    include_directories(${SDL2_INCLUDE_DIRS})
    list(APPEND Display_SRCS
        ${CMAKE_CURRENT_SOURCE_DIR}/SDL/DisplaySDL.h
        ${CMAKE_CURRENT_SOURCE_DIR}/SDL/DisplaySDL.cpp
    )
else()
    set(SAMPLE_WITH_SDL OFF)
endif()