/*
 * app.ld -- TP4 (Bloque 4): linker script del FW del TP anterior cuando corre
 * detras del bootloader. Partan de ../linker.ld y de la diapo 4.
 *
 */
ENTRY(Reset_Handler)

INCLUDE memory_map.ld

SECTIONS
{
  .text : {
    __fw_start = .;
    KEEP(*(.fw_header))
    *(.text .text.*)
    *(.rodata .rodata.*)
    *(.srodata .srodata.*)
    . = ALIGN(4);
  } > APP_ROM

  .data : {
      _sdata = .;                    /* comienzo en RAM */

      *(.data .data.*)
      *(.sdata .sdata.*)

      . = ALIGN(4);
      _edata = .;                    /* final en RAM */
 } > RAM AT > APP_ROM

 _sidata = LOADADDR(.data);       /* origen de los valores iniciales */

  /* .bss
   *   - en RAM, sin ocupar lugar en la imagen (NOLOAD)
   *   - simbolos _sbss / _ebss
   *   - incluir tambien .sbss y COMMON                                    */
   .bss (NOLOAD) : {
        _sbss = .;

        *(.bss .bss.*)
        *(.sbss .sbss.*)
        *(COMMON)

        . = ALIGN(4);
        _ebss = .;
   } > RAM
  /DISCARD/ : { *(.eh_frame .eh_frame_hdr) }
}


__fw_end  = LOADADDR(.data) + SIZEOF(.data);
__fw_size = __fw_end - __fw_start;
_stack_top = ORIGIN(RAM) + LENGTH(RAM);

ASSERT(_ebss <= _stack_top - 0x400,
       "No queda 1 KiB de RAM para el stack")
