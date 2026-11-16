/*
 * bl.ld -- TP4 (Bloque 4): linker script del BOOTLOADER
 *
 * El BL vive en BL_ROM (memory_map.ld).
 */
ENTRY(_bl_vectors)

INCLUDE memory_map.ld

SECTIONS
{
  .text : {
    KEEP(*(.vectors))
    *(.text .text.*)
    *(.rodata .rodata.* .srodata .srodata.*)
    . = ALIGN(4);
  } > BL_ROM

  .data : {
    *(.data .data.* .sdata .sdata.*)
  } > RAM

  .bss : {
    *(.bss .bss.* .sbss .sbss.*)
    *(COMMON)
  } > RAM

  ASSERT(SIZEOF(.data) == 0, "El BL no debe usar .data ni .sdata")
  ASSERT(SIZEOF(.bss) == 0, "El BL no debe usar .bss ni .sbss")

  /DISCARD/ : { *(.eh_frame .eh_frame_hdr) }
}

    _bl_stack_top = ORIGIN(RAM) + LENGTH(RAM);

/* (YA ESTA HECHO) Donde espera el BL encontrar al FW: bl_main.c los usa como extern */
__fw_slot_start = ORIGIN(APP_ROM);
__fw_slot_size  = LENGTH(APP_ROM);

ASSERT(_bl_vectors == ORIGIN(BL_ROM), "La tabla de vectores debe comenzar en BL_ROM")
