#
# Simple test loader to verify Multiboot2 boot and serial output
#

.set MB2_HEADER_MAGIC, 0xE85250D6
.set MB2_ARCH,         0
.set MB2_LEN,          24
.set MB2_CHECKSUM,     0x17ADAF12

    .section .multiboot2
    .align 8
    .long MB2_HEADER_MAGIC
    .long MB2_ARCH
    .long MB2_LEN
    .long MB2_CHECKSUM
    # End tag (type=0, size=8)
    .long 0
    .long 8

    .section .text
    .code32
    .globl _start
_start:
    cli
    
    # Immediately try to print 'A' to serial port
    mov $0x3F8, %dx     # COM1 base
    mov $65, %al        # 'A'
    out %al, %dx
    
    # Print a few more characters to be sure
    mov $66, %al        # 'B'
    out %al, %dx
    mov $67, %al        # 'C'
    out %al, %dx
    mov $10, %al        # newline
    out %al, %dx
    
    # Infinite loop
    hlt
    jmp .
