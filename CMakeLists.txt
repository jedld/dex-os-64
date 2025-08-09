cmake_minimum_required(VERSION 3.20)
project(dex-os-64 C ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_EXTENSIONS OFF)

# Output dir
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# UEFI apps for x86_64 and AArch64
add_subdirectory(src/uefi)

# 32-bit multiboot loader and 64-bit kernel
add_subdirectory(src/loader32)
add_subdirectory(src/kernel64)


