.section .text.start, "ax"
.code64
.globl kstart64
.extern kmain64

kstart64:
    # On entry from loader: RDI = mb2 info pointer
    # Switch to a kernel-owned 64-bit stack to avoid relying on the loader's
    # stack, which may be outside our later identity map after switching CR3.
    lea kstack_top(%rip), %rsp
    sub $8, %rsp             # SysV ABI: RSP % 16 == 8 prior to 'call'
    
    # Early marker: write 'K' at top-left of VGA text buffer (white on black)
    mov $0xB8000, %rax
    movw $0x0F4B, (%rax)

    # RDI already has the mb2 info pointer
    call kmain64
    
    # If returns, hang
.Lhang:
    hlt
    jmp .Lhang

    .section .bss
    .align 16
    .globl kstack
kstack:
    .space 65536              # 64 KiB kernel stack (increased from 16 KiB)
    .globl kstack_top
kstack_top:
