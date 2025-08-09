OUTPUT_FORMAT(elf32-i386)
ENTRY(_start)
SECTIONS {
  . = 1M;
  .multiboot2 : { *(.multiboot2) }
  .text : { *(.text*) }
  .rodata : { *(.rodata*) }
  .data : { *(.data*) }
  .bss : { *(.bss*) *(COMMON) }
}
