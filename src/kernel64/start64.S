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
    # Serial debug: print 'K' to COM1 to confirm entry (wait for THR empty)
    mov $0x3FD, %dx            # LSR
.Lwait_k:
    in %dx, %al
    test $0x20, %al            # THRE bit
    jz .Lwait_k
    mov $0x3F8, %dx            # THR
    mov $75, %al               # 'K'
    out %al, %dx
    # Debug marker before call 'W'
    mov $0x3FD, %dx
.Lwait_w:
    in %dx, %al
    test $0x20, %al
    jz .Lwait_w
    mov $0x3F8, %dx
    mov $87, %al               # 'W'
    out %al, %dx
    # RDI already has the mb2 info pointer
    call kmain64
    # Debug marker after call (if we get here) 'Z'
    mov $0x3FD, %dx
.Lwait_z:
    in %dx, %al
    test $0x20, %al
    jz .Lwait_z
    mov $0x3F8, %dx
    mov $90, %al               # 'Z'
    out %al, %dx
    # If returns, hang
.Lhang:
    hlt
    jmp .Lhang
