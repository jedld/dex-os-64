cmake_minimum_required(VERSION 3.20)
project(loader32 ASM)

enable_language(ASM)

add_executable(loader32 loader.S)
set_target_properties(loader32 PROPERTIES OUTPUT_NAME loader32.elf)

target_compile_options(loader32 PRIVATE -m32 -ffreestanding -fno-pic -fno-pie -fno-asynchronous-unwind-tables -nostdlib -nostartfiles)
# Provide a minimal linker script for 32-bit flat binary
# We'll add it below in src/loader32/linker.ld

target_link_options(loader32 PRIVATE -fuse-ld=lld -m32 -nostdlib -Wl,-static -Wl,--no-undefined -Wl,--build-id=none -Wl,-T,${CMAKE_CURRENT_SOURCE_DIR}/linker.ld -Wl,-no-pie)
