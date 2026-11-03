# Host tests: app logic that doesn't need a Flipper, built for the computer
# you develop on. The real app sources are compiled against small stand-ins
# for the firmware (shim/).
#
#   cmake -S tests -B build/tests
#   cmake --build build/tests
#   ctest --test-dir build/tests --output-on-failure

cmake_minimum_required(VERSION 3.16)
project(duckwalk_tests C)

set(CMAKE_C_STANDARD 11)
set(APP ${CMAKE_CURRENT_SOURCE_DIR}/../app)

# Windows C runtimes hide M_PI and warn on fopen/strcpy unless asked not to.
add_compile_definitions(_USE_MATH_DEFINES _CRT_SECURE_NO_WARNINGS)

if(MSVC)
    add_compile_options(/W4)
else()
    add_compile_options(-Wall -Wextra -Werror -Wno-unused-function)
endif()

add_library(shim STATIC shim/shim.c)
target_include_directories(shim PUBLIC shim ${APP})

find_library(MATH m)

add_executable(test_walk test_walk.c ${APP}/walk.c)
target_link_libraries(test_walk shim $<$<BOOL:${MATH}>:${MATH}>)

add_executable(test_settings test_settings.c ${APP}/settings.c)
target_link_libraries(test_settings shim)

add_executable(test_helper test_helper.c fake_link.c ${APP}/helper.c ${APP}/keep_awake.c ${APP}/walk.c)
target_link_libraries(test_helper shim $<$<BOOL:${MATH}>:${MATH}>)

enable_testing()
add_test(NAME walk COMMAND test_walk)
add_test(NAME helper COMMAND test_helper)
add_test(NAME settings COMMAND test_settings WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
