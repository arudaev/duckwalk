# Duckwalk helper for Windows. Plain C and Win32, no dependencies.
#
#   cmake -S desktop/helper -B build/helper
#   cmake --build build/helper
#
# MinGW gcc or Visual Studio both work.

cmake_minimum_required(VERSION 3.16)
project(duckwalk_helper C)

set(CMAKE_C_STANDARD 11)

if(NOT WIN32)
    message(FATAL_ERROR "The Duckwalk helper is Windows only for now")
endif()

add_executable(duckwalk-helper helper.c)
target_compile_definitions(duckwalk-helper PRIVATE UNICODE _UNICODE _CRT_SECURE_NO_WARNINGS)
target_link_libraries(duckwalk-helper user32 setupapi)
if(MSVC)
    target_compile_options(duckwalk-helper PRIVATE /W4)
else()
    target_compile_options(duckwalk-helper PRIVATE -Wall -Wextra -Wno-missing-field-initializers)
    target_link_options(duckwalk-helper PRIVATE -static)
endif()
