cmake_minimum_required(VERSION 3.8)
project(MMP-Core DESCRIPTION "Rockchip MMP(Multi Media Plugin) Demo")

# Hint : modify if needed
# set(OPENGL_FOUND ON CACHE INTERNAL "")
# set(OPENGL_LIBRARIES "/usr/lib/aarch64-linux-gnu/libmali-valhall-g610-g6p0-x11-gbm.so" CACHE INTERNAL "")

set(Test_LIBS Mmp::Common Mmp::Codec Poco::Foundation Poco::Util Mmp::GL Mmp::Windows Mmp::PG Mmp::SL)

add_subdirectory(MMP-Core)
add_subdirectory(Display)

add_executable(test_encoder ${CMAKE_CURRENT_SOURCE_DIR}/test_encoder.cpp)
target_link_libraries(test_encoder ${Test_LIBS})

add_executable(test_decoder ${CMAKE_CURRENT_SOURCE_DIR}/test_decoder.cpp)
target_link_libraries(test_decoder ${Test_LIBS} Display)

add_executable(test_transcode ${CMAKE_CURRENT_SOURCE_DIR}/test_transcode.cpp)
target_link_libraries(test_transcode ${Test_LIBS})

add_executable(test_compositor ${CMAKE_CURRENT_SOURCE_DIR}/test_compositor.cpp)
target_link_libraries(test_compositor ${Test_LIBS} Display)
