/* doom.ld -- Doom's link: four blocks of memory, two files (README.md, "Memory").
 *
 *   APP  0x03030000  112 KiB    core A: the program region. DOOM.APP, which GILB loads and checks:
 *                               the header, the first stage (gilb_main.c), the hot code (the
 *                               renderer, the game logic) and its constants.
 *   AHI  0x03077900  161.75 KiB core A: the fonts window, the audio DMA list and the PCM ring
 *                               (api->more_mem_a, API version 3): the math tables, the frame, the
 *                               RGB565 bands for the panel.
 *   BLO  0x01000000  128 KiB    core B's RAM (api->more_mem, version 2): .data and .bss, then the
 *                               zone. The game's 16-bit pointers (i_memory.h) reach only
 *                               0x01000000..0x0103FFFF, so every variable lives here.
 *   BHI  0x01021000  252 KiB    core B's RAM: the cold code (menus, level set-up, intermission,
 *                               finale, the automap, cheats), info.c's tables, then the caches
 *                               (gilb_main.c).
 *
 * Code in core B's RAM runs 4.8x slower and data there reads 1.6x slower (docs/DOOM_PLAN.md).
 * AHI, BLO and BHI come from DOOM.BIN, which the first stage reads after asking for the memory
 * (tools/pack.py writes both files from this link's segments). Calls between APP and BHI are
 * 32 MiB apart, past a Thumb BL: the linker puts long-branch veneers in.
 */
MEMORY
{
    APP (rwx) : ORIGIN = 0x03030000, LENGTH = 0x1C000
    AHI (rwx) : ORIGIN = 0x03077900, LENGTH = 0x28700
    BLO (rwx) : ORIGIN = 0x01000000, LENGTH = 0x20000
    BHI (rwx) : ORIGIN = 0x01021000, LENGTH = 0x3F000
}

PHDRS
{
    app PT_LOAD;
    ahi PT_LOAD;
    blo PT_LOAD;
    bhi PT_LOAD;
}

ENTRY(gilb_hdr)


SECTIONS
{
    .text :
    {
        KEEP(*(.gilb_hdr))
        . = ALIGN(4);
        *(.stage1 .stage1.*)
        EXCLUDE_FILE(*m_menu.o *f_finale.o *wi_stuff.o *p_setup.o *am_map.o *m_cheat.o *p_genlin.o
                     *p_switch.o *p_telept.o *p_inter.o *s_sound.o *r_data.o *printf.o *d_items.o
                     *sounds.o *f_wipe.o *gilb_cold.o *info.o *tables.o)
            *(.text .text.* .rodata .rodata.*)
        *(.glue_7 .glue_7t .vfp11_veneer .v4_bx)
        __exidx_start = .;           /* (libgcc asks; nothing unwinds) */
        __exidx_end = .;
        . = ALIGN(4);
    } > APP :app
    __load_end = .;
    .app_bss (NOLOAD) :
    {
        *(.app_bss .app_bss.*)
        . = ALIGN(4);
    } > APP :app
    __mem_end = .;

    .ahi :
    {
        __ahi_start = .;
        *tables.o(.rodata .rodata.*)
        *(.cold.text .cold.text.*)
        /* the cold code: menus, level set-up, the automap, intermission, finale, cheats,
           specials, the lump bookkeeping, printf */
        *m_menu.o(.text .text.* .rodata .rodata.*)
        *f_finale.o(.text .text.* .rodata .rodata.*)
        *wi_stuff.o(.text .text.* .rodata .rodata.*)
        *p_setup.o(.text .text.* .rodata .rodata.*)
        *am_map.o(.text .text.* .rodata .rodata.*)
        *m_cheat.o(.text .text.* .rodata .rodata.*)
        *p_genlin.o(.text .text.* .rodata .rodata.*)
        *p_switch.o(.text .text.* .rodata .rodata.*)
        *p_telept.o(.text .text.* .rodata .rodata.*)
        *p_inter.o(.text .text.* .rodata .rodata.*)
        *s_sound.o(.text .text.* .rodata .rodata.*)
        *r_data.o(.text .text.* .rodata .rodata.*)
        *printf.o(.text .text.* .rodata .rodata.*)
        *d_items.o(.text .text.* .rodata .rodata.*)
        *f_wipe.o(.text .text.* .rodata .rodata.*)
        *gilb_cold.o(.text .text.* .rodata .rodata.*)
        . = ALIGN(4);
    } > AHI :ahi
    .ahi_bss (NOLOAD) :
    {
        *(.fb .fb.*)
        *(.bands .bands.*)
        . = ALIGN(8);
        __ahi_free = .;              /* the WAD's read cache: from here to the end of AHI */
    } > AHI :ahi

    .data :
    {
        __blo_start = .;
        . = . + 8;                   /* (a 16-bit pointer of 0 is NULL: nothing at 0x01000000) */
        *(.data .data.*)
        . = ALIGN(4);
        *info.o(.rodata .rodata.*)   /* the states and things: read every tic, a little slower */
        *sounds.o(.rodata .rodata.*)
        . = ALIGN(4);
    } > BLO :blo
    .bss (NOLOAD) :
    {
        *(.openings_bss .openings_bss.*)
        *(.bss .bss.*)
        *(COMMON)
        . = ALIGN(8);
        __blo_free = .;              /* the zone: from here to the end of BLO (z_zone.c) */
    } > BLO :blo

    /* BHI: nothing linked; all of it is the game's "flash" (gilb_main.c). */

    /DISCARD/ : { *(.ARM.exidx*) *(.ARM.extab*) *(.comment) *(.init) *(.fini) *(.init_array*) *(.fini_array*) }
}

__app_end = ORIGIN(APP) + LENGTH(APP);
__ahi_end = ORIGIN(AHI) + LENGTH(AHI);
__blo_end = ORIGIN(BLO) + LENGTH(BLO);
__bhi_start = ORIGIN(BHI);
__bhi_end = ORIGIN(BHI) + LENGTH(BHI);
ASSERT(gilb_hdr == ORIGIN(APP), "the header must be the first thing in the program")
ASSERT(__blo_end - __blo_free >= 62 * 1024, "the zone needs 62 KiB (the MG24's) at least")
ASSERT(__ahi_end - __ahi_free >= 16 * 1024, "the sound ring (8 KiB) and the WAD's pages need 16 KiB at least")
