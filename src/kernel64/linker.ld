ENTRY(kstart64)
SECTIONS
{
  . = 0;
  .text : {
    *(.text.start)
    *(.text .text.*)
    *(.rodata*)
    *(.data*)
  }
  .bss : { *(.bss*) *(COMMON) }
  /DISCARD/ : {
    *(.note.gnu.build-id)
    *(.comment)
    *(.eh_frame)
  }
}
