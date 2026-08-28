# Toolchain CMake para compilar DLL Windows x64 com ABI MSVC, no Linux.
# Usa clang-cl + lld-link contra o CRT/SDK baixados pelo xwin.
# ABI MSVC e obrigatoria: o CommonLibSF e o proprio Starfield.exe sao MSVC.
#
# Uso: cmake -B build -G Ninja -DCMAKE_TOOLCHAIN_FILE=/home/raelg/sfse-toolchain/msvc.cmake

set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR AMD64)

set(XWIN /home/raelg/sfse-toolchain/xwin)

set(CMAKE_C_COMPILER clang-cl)
set(CMAKE_CXX_COMPILER clang-cl)
set(CMAKE_LINKER lld-link)
set(CMAKE_RC_COMPILER llvm-rc)
set(CMAKE_MT llvm-mt)
set(CMAKE_AR llvm-lib)

# o SDK da Microsoft vem com maiusculas/minusculas inconsistentes; num
# filesystem case-sensitive isso quebra os #include. -vfsoverlay resolve.
set(_inc "/imsvc${XWIN}/crt/include \
/imsvc${XWIN}/sdk/include/ucrt \
/imsvc${XWIN}/sdk/include/um \
/imsvc${XWIN}/sdk/include/shared")

# O xwin NAO baixa o CRT de debug (msvcrtd.lib) — a Microsoft nao redistribui.
# Entao o runtime e sempre o release (/MD); Debug aqui significa so simbolos.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreadedDLL")
set(CMAKE_TRY_COMPILE_CONFIGURATION Release)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "" FORCE)
endif()
set(CMAKE_CXX_FLAGS_DEBUG_INIT "/Zi /Ob0 /Od")
set(CMAKE_C_FLAGS_DEBUG_INIT "/Zi /Ob0 /Od")

set(_flags "-fuse-ld=lld-link /EHsc -Wno-unused-command-line-argument ${_inc}")
set(CMAKE_C_FLAGS_INIT "${_flags}")
set(CMAKE_CXX_FLAGS_INIT "${_flags}")

set(_libs "/libpath:${XWIN}/crt/lib/x86_64 \
/libpath:${XWIN}/sdk/lib/ucrt/x86_64 \
/libpath:${XWIN}/sdk/lib/um/x86_64")

set(CMAKE_EXE_LINKER_FLAGS_INIT "${_libs}")
set(CMAKE_SHARED_LINKER_FLAGS_INIT "${_libs}")

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM BEFORE)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
