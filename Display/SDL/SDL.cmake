cmake_minimum_required(VERSION 3.8)

set(SDL_SRCS)
set(SDL_INCS)

if (NOT SDL2_FOUND)
    find_package(SDL2 CONFIG REQUIRED)
endif()

message("-- Found SDL2")
message("  -- SDL INCS : ${SDL2_INCLUDE_DIRS}")
message("  -- SDL LIB DIR : ${SDL2_LIBRARY_DIRS}")
message("  -- SDL LIBS : ${SDL2_LIBRARIES}")
list(APPEND SDL_INCS ${SDL2_INCLUDE_DIRS})

include_directories(${SDL2_INCLUDE_DIRS})

list(APPEND SDL_INCS
    ${MMP_PRO_TOP_DIR}
    ${MMP_CORE_PRO_TOP_DIR}
)
list(APPEND SDL_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/Display/SDL/DisplaySDL.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Display/SDL/DisplaySDL.cpp
)

add_library(Sample_SDL OBJECT ${SDL_SRCS})
add_library(Sample::SDL ALIAS Sample_SDL)
target_include_directories(Sample_SDL PUBLIC ${SDL_INCS})

list(APPEND Common_LIBS
    Poco::Foundation
    Mmp::Common
    ${SDL2_LIBRARIES}
    Sample::SDL
)
list(APPEND Common_LIBS_DIR
    ${SDL2_LIBRARY_DIRS}
)