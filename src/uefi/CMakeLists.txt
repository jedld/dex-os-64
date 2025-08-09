# Build UEFI applications for x86_64 and AArch64

# Common source
set(UEFI_SOURCES efi_main.c)

# x86_64 build
add_executable(uefi_x86_64 ${UEFI_SOURCES})
set_target_properties(uefi_x86_64 PROPERTIES OUTPUT_NAME BOOTX64 SUFFIX .EFI)

# AArch64 build
add_executable(uefi_aarch64 ${UEFI_SOURCES})
set_target_properties(uefi_aarch64 PROPERTIES OUTPUT_NAME BOOTAA64 SUFFIX .EFI)

# Install layout for USB/ISO staging
install(TARGETS uefi_x86_64 RUNTIME DESTINATION EFI/BOOT OPTIONAL)
install(TARGETS uefi_aarch64 RUNTIME DESTINATION EFI/BOOT OPTIONAL)
install(FILES ${CMAKE_SOURCE_DIR}/boot/grub/grub.cfg DESTINATION boot/grub)
