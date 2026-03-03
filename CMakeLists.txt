cmake_minimum_required(VERSION 3.16)
project(d3d9_proxy)

# ==========================================================
# Универсальная сборка: Linux (MinGW кросс-компиляция) и Windows (MSVC / MinGW)
#
# Linux -> Windows DLL (кросс-компиляция):
#   mkdir build && cd build
#   cmake .. -DCMAKE_TOOLCHAIN_FILE=../toolchain-mingw32.cmake
#   make
#
# Windows, MSVC (Visual Studio 2019/2022):
#   mkdir build && cd build
#   cmake .. -G "Visual Studio 17 2022" -A Win32
#   cmake --build . --config Release
#
# Windows, MinGW:
#   mkdir build && cd build
#   cmake .. -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
#   mingw32-make
# ==========================================================

add_library(d3d9 SHARED src/d3d9_proxy.cpp)

target_compile_features(d3d9 PRIVATE cxx_std_17)

if(MSVC)
    target_compile_options(d3d9 PRIVATE /O2 /W3)
    set_target_properties(d3d9 PROPERTIES
        LINK_FLAGS "/DEF:\"${CMAKE_CURRENT_SOURCE_DIR}/d3d9.def\""
    )
else()
    # MinGW (Windows или Linux кросс-компиляция)
    target_compile_options(d3d9 PRIVATE -O2 -m32 -Wall)
    target_link_options(d3d9 PRIVATE
        -static-libgcc
        -static-libstdc++
        -Wl,--kill-at
    )
    target_link_libraries(d3d9 PRIVATE -ld3d9 -ldxguid)
endif()

set_target_properties(d3d9 PROPERTIES
    PREFIX ""
    SUFFIX ".dll"
    OUTPUT_NAME "d3d9"
)
