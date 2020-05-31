cmake_minimum_required(VERSION 3.16)
project(opus_trivial C)

set(CMAKE_C_STANDARD 99)

include(${CMAKE_BINARY_DIR}/conanbuildinfo.cmake)
conan_basic_setup()

add_executable(opus_trivial main.c)
target_link_libraries(opus_trivial ${CONAN_LIBS})