cmake_minimum_required(VERSION 3.20)
project(kernel64 C ASM)

enable_language(ASM)

set(SRCS
  start64.S
  kmain64.c
  console.c
  input.c
  memtest.c
  ../kernel/mm/pmm.c
  ../kernel/mm/vmm.c
  ../kernel/mm/kmalloc.c
  sched/sched.c
)

add_executable(kernel64_elf ${SRCS})
set_target_properties(kernel64_elf PROPERTIES OUTPUT_NAME kernel64.elf)

target_compile_options(kernel64_elf PRIVATE -ffreestanding -fpie -mno-sse -mno-mmx -m64)

target_link_options(kernel64_elf PRIVATE -fuse-ld=lld -nostdlib -static -Wl,-T,${CMAKE_CURRENT_SOURCE_DIR}/linker.ld -Wl,-no-pie -Wl,--no-dynamic-linker)

add_custom_command(TARGET kernel64_elf POST_BUILD
  COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:kernel64_elf> ${CMAKE_CURRENT_BINARY_DIR}/kernel64.bin
  BYPRODUCTS ${CMAKE_CURRENT_BINARY_DIR}/kernel64.bin
  COMMENT "objcopy kernel64.elf -> kernel64.bin")
