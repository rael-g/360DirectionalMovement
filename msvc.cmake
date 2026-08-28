# CMake toolchain for building Windows x64 DLLs with the MSVC ABI on Linux.
# Drives clang-cl and lld-link against the CRT and SDK fetched by xwin.
# The MSVC ABI is mandatory: Starfield.exe and SFSE are both MSVC.
#
# Usage: cmake -B build -G Ninja -DCMAKE_TOOLCHAIN_FILE=/path/to/msvc.cmake

set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR AMD64)

# Derived from this file's own location so the checkout can live anywhere.
# Override with -DXWIN=<path> if the SDK was splatted elsewhere.
if(NOT DEFINED XWIN)
    set(XWIN ${CMAKE_CURRENT_LIST_DIR}/xwin)
endif()

set(CMAKE_C_COMPILER clang-cl)
set(CMAKE_CXX_COMPILER clang-cl)
set(CMAKE_LINKER lld-link)
set(CMAKE_RC_COMPILER llvm-rc)
set(CMAKE_MT llvm-mt)
set(CMAKE_AR llvm-lib)

# The Microsoft SDK ships with inconsistent capitalisation, which breaks
# #include on a case sensitive filesystem. -vfsoverlay maps it back.
set(_inc "/imsvc${XWIN}/crt/include \
/imsvc${XWIN}/sdk/include/ucrt \
/imsvc${XWIN}/sdk/include/um \
/imsvc${XWIN}/sdk/include/shared")

# xwin cannot fetch the debug CRT (msvcrtd.lib) because Microsoft does not
# redistribute it, so the runtime is always the release one (/MD). A Debug
# build here means symbols only.
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
