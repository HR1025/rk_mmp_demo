cmake_minimum_required(VERSION 3.8)
project(MMP-Core DESCRIPTION "Rockchip MMP(Multi Media Plugin) Demo")

set(Test_LIBS Mmp::Common Mmp::Codec Poco::Foundation)

add_subdirectory(MMP-Core)

add_executable(test_encoder ${CMAKE_CURRENT_SOURCE_DIR}/test_encoder.cpp)
target_link_libraries(test_encoder ${Test_LIBS})
