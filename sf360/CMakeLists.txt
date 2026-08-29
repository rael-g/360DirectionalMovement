cmake_minimum_required(VERSION 3.21)
project(sf360 VERSION 0.1.1 LANGUAGES C CXX)

# CMake versions are numeric only, so the pre-release suffix is carried apart.
# Empty on a final release.
set(SF360_PRERELEASE "-rc8")

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Headers from the official SFSE source tree. Nothing is reimplemented.
set(SFSE_SRC ${CMAKE_CURRENT_SOURCE_DIR}/../sfse-src)

# The plugin is C. sfse_shim.cpp is the single C++ translation unit, needed
# because SFSE hands out per-frame work through an abstract class.
add_library(sf360 SHARED
    src/sfse_shim.cpp
    src/address_library.c
    src/config.c
    src/diagnostics.c
    src/game.c
    src/layout_check.c
    src/log.c
    src/movement_director.c
    src/paths.c
    src/plugin.c
    src/restraint.c
    src/rotator.c)

target_include_directories(sf360 PRIVATE
    ${SFSE_SRC}/sfse ${SFSE_SRC}/sfse_common ${SFSE_SRC})
target_link_libraries(sf360 PRIVATE ole32.lib shell32.lib)
target_compile_definitions(sf360 PRIVATE SF360_VERSION="${PROJECT_VERSION}${SF360_PRERELEASE}")

# Static CRT: a dynamically linked one would make the plugin depend on the
# Visual C++ redistributable, and a machine without it fails to load the DLL
# with no explanation beyond SFSE reporting the plugin as broken.
set_property(TARGET sf360 PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded")

# SFSE requires the DLL name to match the file in SFSE/Plugins exactly.
set_target_properties(sf360 PROPERTIES PREFIX "" OUTPUT_NAME "sf360")
