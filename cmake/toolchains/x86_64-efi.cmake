set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR x86_64)

# Use clang + lld to produce PE/COFF directly (Windows COFF target)
set(CMAKE_C_COMPILER clang)
set(CMAKE_ASM_COMPILER clang)

set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

set(CMAKE_C_FLAGS_INIT "-target x86_64-pc-windows-msvc -ffreestanding -fshort-wchar -fno-stack-protector -fno-builtin -fno-pic -fno-pie -mno-red-zone -I${CMAKE_SOURCE_DIR}/src/uefi")
set(CMAKE_EXE_LINKER_FLAGS_INIT "-fuse-ld=lld -nostdlib -Wl,/nodefaultlib -Wl,/subsystem:efi_application -Wl,/entry:efi_main -Wl,/machine:x64")
