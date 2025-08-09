set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR aarch64)

set(CMAKE_C_COMPILER clang)
set(CMAKE_ASM_COMPILER clang)

set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

set(CMAKE_C_FLAGS_INIT "-target aarch64-pc-windows-msvc -ffreestanding -fshort-wchar -fno-stack-protector -fno-builtin -fno-pic -fno-pie -I${CMAKE_SOURCE_DIR}/src/uefi")
set(CMAKE_EXE_LINKER_FLAGS_INIT "-fuse-ld=lld -nostdlib -Wl,/nodefaultlib -Wl,/subsystem:efi_application -Wl,/entry:efi_main -Wl,/machine:arm64")
