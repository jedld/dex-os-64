ENTRY(kstart64)
SECTIONS
{
  . = 0;
  .text : {
    __kernel_start = .;
    *(.text.start)
    *(.text .text.*)
    *(.rodata*)
    *(.data*)
  }
  .bss : { *(.bss*) *(COMMON) }
  __kernel_end = .;
  /DISCARD/ : {
    *(.note.gnu.build-id)
    *(.comment)
    *(.eh_frame)
  }
}
