set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR i386)

set(CMAKE_C_COMPILER clang)
set(CMAKE_ASM_COMPILER clang)

set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

set(CMAKE_C_FLAGS_INIT "-m32 -ffreestanding -fno-stack-protector -fno-pic -fno-pie -fno-builtin -march=i386 -mno-sse -mno-mmx")
set(CMAKE_ASM_FLAGS_INIT "-m32")
set(CMAKE_EXE_LINKER_FLAGS_INIT "-fuse-ld=lld -m32 -nostdlib -Wl,-static -Wl,--no-undefined")
