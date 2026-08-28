cmake_minimum_required(VERSION 3.21)
project(sf360 CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Cabecalhos vindos do fonte oficial do SFSE 0.2.21, extraido do proprio mod
# instalado (cmm/mods/f5b3.../src/sfse-0.2.21.tar.gz). Nada aqui e reimplementado.
set(SFSE_SRC ${CMAKE_CURRENT_SOURCE_DIR}/../sfse-src)

add_library(sf360 SHARED src/main.cpp)
target_include_directories(sf360 PRIVATE ${SFSE_SRC}/sfse ${SFSE_SRC}/sfse_common ${SFSE_SRC})
target_link_libraries(sf360 PRIVATE ole32.lib shell32.lib)

# O SFSE exige que a DLL se chame exatamente como o arquivo em SFSE/Plugins.
set_target_properties(sf360 PROPERTIES PREFIX "" OUTPUT_NAME "sf360")
