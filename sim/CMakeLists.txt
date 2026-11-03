# Screen simulator: the app's real draw code on a computer, drawn with the
# firmware's own copy of u8g2 (the library behind the Flipper's canvas, with
# its fonts) and saved as PNGs. Needs a C compiler, CMake, zlib, and the
# firmware's lib/u8g2 folder:
#
#   git clone --depth 1 --filter=blob:none --sparse https://github.com/flipperdevices/flipperzero-firmware fw
#   git -C fw sparse-checkout set lib/u8g2
#   cmake -S sim -B build/sim -DU8G2_DIR=fw/lib/u8g2
#   cmake --build build/sim
#   build/sim/duckwalk_sim build/screens

cmake_minimum_required(VERSION 3.16)
project(duckwalk_sim C)

set(CMAKE_C_STANDARD 11)
set(APP ${CMAKE_CURRENT_SOURCE_DIR}/../app)
set(SHIM ${CMAKE_CURRENT_SOURCE_DIR}/../tests/shim)

set(U8G2_DIR "" CACHE PATH "The firmware's lib/u8g2 folder")
if(NOT EXISTS "${U8G2_DIR}/u8g2.h")
    message(FATAL_ERROR "Set -DU8G2_DIR to flipperzero-firmware/lib/u8g2 (see the top of sim/CMakeLists.txt)")
endif()
find_package(ZLIB REQUIRED)

add_compile_definitions(_USE_MATH_DEFINES _CRT_SECURE_NO_WARNINGS)

# u8g2 minus the firmware's glue to the real display (it needs furi_hal).
file(GLOB U8G2_SOURCES ${U8G2_DIR}/*.c)
list(FILTER U8G2_SOURCES EXCLUDE REGEX "u8g2_glue\\.c$")
add_library(u8g2 STATIC ${U8G2_SOURCES} u8g2_stubs.c)
target_include_directories(u8g2 PUBLIC ${U8G2_DIR})
# The firmware's copy leaves out parts of u8g2 that unused functions refer
# to; like the firmware, drop unused code at link time.
if(NOT MSVC)
    target_compile_options(u8g2 PRIVATE -ffunction-sections -fdata-sections -w)
endif()

# The sprite list: one Icon per app/images/*.png, named as ufbt names them.
file(GLOB IMAGES ${APP}/images/*.png)
set(ICONS_H "#pragma once\n#include <gui/gui.h>\n")
set(ICONS_C "#include \"duckwalk_icons.h\"\n")
set(ICONS_LIST "Icon* const sim_icon_list[] = {\n")
foreach(image ${IMAGES})
    get_filename_component(name ${image} NAME_WE)
    string(APPEND ICONS_H "extern Icon I_${name};\n")
    string(APPEND ICONS_C "Icon I_${name} = {.file = \"${image}\"};\n")
    string(APPEND ICONS_LIST "    &I_${name},\n")
endforeach()
string(APPEND ICONS_LIST "    NULL,\n};\n")
file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/generated/duckwalk_icons.h "${ICONS_H}")
file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/generated/icons_list.c "${ICONS_C}${ICONS_LIST}")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${IMAGES})

add_executable(duckwalk_sim
    main.c canvas.c icons.c png.c link_fake.c notification_fake.c
    ${CMAKE_CURRENT_BINARY_DIR}/generated/icons_list.c
    ${SHIM}/shim.c
    ${APP}/home.c ${APP}/settings_screen.c ${APP}/remote.c ${APP}/preview.c
    ${APP}/pairing.c ${APP}/welcome.c
    ${APP}/keep_awake.c ${APP}/walk.c ${APP}/settings.c ${APP}/helper.c)
target_include_directories(duckwalk_sim PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR} shim ${CMAKE_CURRENT_BINARY_DIR}/generated ${SHIM} ${APP})
target_link_libraries(duckwalk_sim u8g2 ZLIB::ZLIB)
if(NOT WIN32)
    target_link_libraries(duckwalk_sim m)
endif()
if(NOT MSVC)
    # -Wno-format: uint32_t is unsigned long on the Flipper and unsigned int
    # here, so the app's %lu is right there and only looks wrong here.
    target_compile_options(duckwalk_sim PRIVATE -Wall -Wextra -Wno-unused-parameter -Wno-format)
    if(APPLE)
        target_link_options(duckwalk_sim PRIVATE -Wl,-dead_strip)
    else()
        target_link_options(duckwalk_sim PRIVATE -Wl,--gc-sections)
    endif()
endif()
