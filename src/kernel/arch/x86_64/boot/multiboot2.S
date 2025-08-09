#define MULTIBOOT2_HEADER_MAGIC 0xE85250D6
#define MULTIBOOT2_ARCH 0
#define MULTIBOOT2_HEADER_LEN 16
// 0 - (0xE85250D6 + 0 + 16) mod 2^32 = 0x17ADAF1A
#define MULTIBOOT2_CHECKSUM 0x17ADAF1A

    .section .multiboot2
    .align 8
    .long MULTIBOOT2_HEADER_MAGIC
    .long MULTIBOOT2_ARCH
    .long MULTIBOOT2_HEADER_LEN
    .long MULTIBOOT2_CHECKSUM

    .section .text
    .code32
    .globl _start
_start:
    cli
    # Multiboot2 entry (GRUB will pass multiboot info in EBX)
    mov $stack_top, %esp
    push %ebx
    call kmain
.hang:
    hlt
    jmp .hang

    .section .bss
    .align 16
stack:
    .space 4096
stack_top:
