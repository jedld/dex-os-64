.section .text.start, "ax"
.code64
.globl kstart64
.extern kmain64

kstart64:
    # On entry from loader: RDI = mb2 info pointer, stack valid
    # Ensure SysV ABI stack alignment: RSP % 16 == 8 prior to 'call'
    mov %rsp, %rax
    and $~0xF, %rsp          # align to 16 bytes
    sub $8, %rsp             # make RSP % 16 == 8 (for call)
    
    # RDI already has the mb2 info pointer
    call kmain64
    
    # If returns, hang
.Lhang:
    hlt
    jmp .Lhang
