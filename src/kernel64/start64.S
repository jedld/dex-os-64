.text
.code64
.globl kstart64
kstart64:
    # Use 32-bit mov to dx to avoid 16-bit prefix
    mov $0x3F8, %edx
    mov $75, %eax        # 'K' - kernel64 entry marker
    out %al, %dx
    mov $54, %eax        # '6'
    out %al, %dx
    mov $52, %eax        # '4'
    out %al, %dx
    mov $33, %eax        # '!'
    out %al, %dx
    
    hlt
    jmp .
