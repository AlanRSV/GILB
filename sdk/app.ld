/* app.ld -- a GILB program's link (sdk/include/gilb_app.h): one block at GILB_APP_BASE, the header
   first, code, constants and data as they are in the file (GILB loads the file there as it is, so
   .data needs no copy), then .bss, which GILB zeroes. 112 KiB in all; the stack is GILB's own. */
MEMORY
{
    APP (rwx) : ORIGIN = 0x03030000, LENGTH = 0x1C000
}

ENTRY(gilb_hdr)

SECTIONS
{
    .text :
    {
        KEEP(*(.gilb_hdr))
        . = ALIGN(4);
        __movable_start = .;             /* code a program copies elsewhere itself (more_mem) */
        KEEP(*(.movable .movable.*))
        . = ALIGN(4);
        __movable_end = .;
        *(.text .text.*)
        *(.rodata .rodata.*)
        . = ALIGN(4);
    } > APP
    .data :
    {
        *(.data .data.*)
        . = ALIGN(4);
    } > APP
    __load_end = .;
    .bss (NOLOAD) :
    {
        *(.bss .bss.*)
        *(COMMON)
        . = ALIGN(4);
    } > APP
    __mem_end = .;
    /DISCARD/ : { *(.ARM.exidx*) *(.ARM.extab*) *(.comment) }
}
ASSERT(gilb_hdr == ORIGIN(APP), "the header must be the first thing in the program")
