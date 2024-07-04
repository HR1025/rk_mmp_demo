cmake_minimum_required(VERSION 3.8)
project(MMP-Core DESCRIPTION "Rockchip MMP(Multi Media Plugin) Demo")

set(Test_LIBS Mmp::Common Mmp::Codec Poco::Foundation Poco::Util)

add_subdirectory(MMP-Core)
add_subdirectory(Display)

add_executable(test_encoder ${CMAKE_CURRENT_SOURCE_DIR}/test_encoder.cpp)
target_link_libraries(test_encoder ${Test_LIBS})

add_executable(test_decoder ${CMAKE_CURRENT_SOURCE_DIR}/test_decoder.cpp)
target_link_libraries(test_decoder ${Test_LIBS} Display)

add_executable(test_transcode ${CMAKE_CURRENT_SOURCE_DIR}/test_transcode.cpp)
target_link_libraries(test_transcode ${Test_LIBS})