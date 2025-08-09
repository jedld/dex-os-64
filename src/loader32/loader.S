#
# loader.S — 32-bit Multiboot2 loader that enables x86_64 long mode and jumps
#             to a 64-bit kernel module entry (kstart64) at module start.
#             Passes Multiboot2 info pointer in RDI and sets a 64-bit stack.
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
    # On entry, EBX = Multiboot2 info pointer (GRUB)
    mov %ebx, mb2_ptr

    # Set up a temporary 32-bit stack
    mov $stack32_top, %esp

    # Simple serial output - just print chars directly
    mov $0x3F8, %dx     # COM1 base
    mov $65, %al        # 'A' - entry
    out %al, %dx
    mov $66, %al        # 'B' - stack set
    out %al, %dx

    # Setup a simple GDT with 64-bit code and data segments
    lgdt gdt_desc
    mov $0x10, %ax      # data selector
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %ss

    # Build identity-mapped 1 GiB with 2 MiB pages
    call build_paging

    mov $67, %al        # 'C' - paging built
    out %al, %dx

    # Enable PAE
    mov %cr4, %eax
    or $0x20, %eax      # CR4.PAE
    mov %eax, %cr4

    # Load PML4 into CR3
    mov $pml4, %eax
    mov %eax, %cr3

    # Enable long mode in EFER
    mov $0xC0000080, %ecx    # IA32_EFER
    rdmsr
    or $0x00000100, %eax     # LME
    wrmsr

    # Enable paging
    mov %cr0, %eax
    or $0x80000001, %eax      # PG + PE
    mov %eax, %cr0

    # Far jump to 64-bit mode entry stub
    ljmp $0x08, $long_mode_entry

# Build PML4->PDPT->PD with 2MiB pages covering 1 GiB
build_paging:
    # Zero tables (write full 8 bytes per entry)
    xor %eax, %eax
    mov $512, %ecx
    mov $pml4, %edi
1:  mov %eax, (%edi)
    mov %eax, 4(%edi)
    add $8, %edi
    loop 1b

    mov $512, %ecx
    mov $pdpt, %edi
2:  mov %eax, (%edi)
    mov %eax, 4(%edi)
    add $8, %edi
    loop 2b

    mov $512, %ecx
    mov $pd, %edi
3:  mov %eax, (%edi)
    mov %eax, 4(%edi)
    add $8, %edi
    loop 3b

    # Setup entries
    # PML4[0] -> PDPT | P | RW
    mov $pdpt, %eax
    or $0x3, %eax
    mov %eax, (pml4)
    mov $0, %eax
    mov %eax, 4+pml4

    # PDPT[0] -> PD | P | RW
    mov $pd, %eax
    or $0x3, %eax
    mov %eax, (pdpt)
    mov $0, %eax
    mov %eax, 4+pdpt

    # PD entries: 512 * 2MiB = 1GiB
    mov $0, %eax            # base phys
    mov $0, %ecx
4:  mov %eax, %edx
    or $0x83, %edx          # P|RW|PS (bit 7)
    mov %ecx, %edi
    lea pd(,%edi,8), %edi
    mov %edx, (%edi)
    mov $0, %edx
    mov %edx, 4(%edi)
    add $0x200000, %eax     # next 2MiB
    inc %ecx
    cmp $512, %ecx
    jne 4b
    ret

    .align 16
long_mode_entry:
    # Now in long mode, use 64-bit code
    .code64
    # Load data segment
    mov $0x10, %ax
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %ss

    # Set up 64-bit stack
    lea stack64_top(%rip), %rsp

    # Retrieve multiboot info pointer saved earlier, pass in RDI
    mov mb2_ptr(%rip), %rax
    mov %rax, %rdi

    # Parse modules to find first module start (kernel64)
    # Very simple scan: find tag type 3
    mov %rdi, %rbx        # rbx = mb2 base
    mov (%rbx), %eax      # total size
    mov $8, %rcx          # offset
    
    # Debug: print total size
    mov $0x3F8, %dx
    mov $84, %al        # 'T' - total size follows
    out %al, %dx
    
find_tag:
    cmp %ecx, %eax
    jle no_module
    lea (%rbx,%rcx), %rdx
    mov 0(%rdx), %r8d     # type
    mov 4(%rdx), %r9d     # size
    
    # Debug: print tag type
    mov $0x3F8, %dx
    mov $116, %al       # 't' - tag type follows  
    out %al, %dx
    
    cmp $3, %r8d          # module tag
    je got_module
    # align next tag to 8
    lea 7(%r9d), %r10d
    and $~7, %r10d
    add %r10d, %ecx
    jmp find_tag

no_module:
    # Debug: no module found
    mov $0x3F8, %dx
    mov $78, %al        # 'N' - no module
    out %al, %dx
    call serial_init64
    lea msg_no_module(%rip), %rsi
    call serial_puts64
    hlt
    jmp no_module

got_module:
    # Debug: print module found
    mov $0x3F8, %dx  
    mov $77, %al        # 'M' - module found
    out %al, %dx
    
    # rsi = module start (at offset 8 in tag, not 16!)
    mov 8(%rdx), %rsi
    mov %rsi, %rax
    
    # Print module address (simplified - just print 'A' if we have one)
    mov $0x3F8, %dx
    mov $65, %al        # 'A' - module address follows
    out %al, %dx
    
    # Test before jump
    mov $90, %al        # 'Z' - about to jump
    out %al, %dx
    
    # For now, let's skip the jump and just print more debug
    mov $33, %al        # '!' - alternative path
    out %al, %dx
    mov $79, %al        # 'O' - OK
    out %al, %dx
    mov $75, %al        # 'K'
    out %al, %dx
    
    # Jump to kernel64 (rsi = start address)
    jmp *%rsi

    .align 16
    .code32

    .align 16
    .globl gdt
 gdt:
    .quad 0x0000000000000000  # null
    # 0x08: 64-bit code: base=0 limit=max, G=1, L=1, D=0, P=1, DPL=0, Code
    .quad 0x00AF9A000000FFFF
    # 0x10: data: base=0 limit=max, G=1, L=0, D=1, P=1, DPL=0, Data RW
    .quad 0x00CF92000000FFFF
 gdt_desc:
     .word gdt_end - gdt - 1
     .long gdt
 gdt_end:

    .align 4096
 pml4:
    .space 4096
 pdpt:
    .space 4096
 pd:
    .space 4096

     .align 16
 stack32:
     .space 8192
 stack32_top:

    .align 16
 stack64:
    .space 4096
 stack64_top:

    .data
    .align 8
 mb2_ptr:
    .quad 0

    .section .rodata
msg_loader_start:
    .asciz "[loader32] start\n"
msg_paging_done:
    .asciz "[loader32] paging ready\n"
msg_no_module:
    .asciz "[loader32] no module found\n"
msg_jump64:
    .asciz "[loader32] jump to kernel64\n"

    .section .text
    .code32
# 32-bit serial helpers (COM1 @ 0x3F8)
serial_init32:
    mov $0x3F8+1, %dx
    mov $0x00, %al
    out %al, %dx
    mov $0x3F8+3, %dx
    mov $0x80, %al
    out %al, %dx
    mov $0x3F8, %dx
    mov $0x03, %al
    out %al, %dx
    mov $0x3F8+3, %dx
    mov $0x03, %al
    out %al, %dx
    mov $0x3F8+2, %dx
    mov $0xC7, %al
    out %al, %dx
    mov $0x3F8+4, %dx
    mov $0x0B, %al
    out %al, %dx
    ret

serial_putc32:
    push %edx
    push %eax
1:  mov $0x3F8+5, %dx
    in %dx, %al
    test $0x20, %al
    jz 1b
    mov $0x3F8, %dx
    pop %eax
    out %al, %dx
    pop %edx
    ret

serial_puts32:
    push %esi
    mov 4(%esp), %esi
2:  lodsb
    test %al, %al
    jz 3f
    push %eax
    call serial_putc32
    jmp 2b
3:  pop %esi
    ret

    .code64
# 64-bit serial helpers to use after long mode entry
serial_init64:
    mov $0x3F8+1, %dx
    mov $0x00, %al
    out %al, %dx
    mov $0x3F8+3, %dx
    mov $0x80, %al
    out %al, %dx
    mov $0x3F8, %dx
    mov $0x03, %al
    out %al, %dx
    mov $0x3F8+3, %dx
    mov $0x03, %al
    out %al, %dx
    mov $0x3F8+2, %dx
    mov $0xC7, %al
    out %al, %dx
    mov $0x3F8+4, %dx
    mov $0x0B, %al
    out %al, %dx
    ret

serial_putc64:
    push %rdx
    push %rax
1:  mov $0x3F8+5, %dx
    in %dx, %al
    test $0x20, %al
    jz 1b
    mov $0x3F8, %dx
    pop %rax
    out %al, %dx
    pop %rdx
    ret

serial_puts64:
    push %rsi
2:  lodsb
    test %al, %al
    jz 3f
    push %rax
    call serial_putc64
    jmp 2b
3:  pop %rsi
    ret
