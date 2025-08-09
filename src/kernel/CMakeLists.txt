cmake_minimum_required(VERSION 3.20)
project(kernel32 C ASM)

enable_language(ASM)

set(KERNEL_SOURCES
  arch/x86_64/boot/multiboot2.S
  kmain.c
  mm/kmalloc.c
  mm/pmm.c
  mm/vmm.c
)

add_executable(kernel32 ${KERNEL_SOURCES})
set_target_properties(kernel32 PROPERTIES OUTPUT_NAME kernel.elf)

target_compile_options(kernel32 PRIVATE -fno-pic -fno-pie)
target_link_options(kernel32 PRIVATE "-Wl,-no-pie" "-Wl,-T,${CMAKE_CURRENT_SOURCE_DIR}/arch/x86_64/linker.ld")
