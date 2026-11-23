# 512 MB vs 8 GB board: what the V3.9.0 firmware tells us

Method (all offline, reproducible):
1. `tools/ghidra/run_headless.sh IMG` imports each image into Ghidra (on Kai) with the real runtime memory map, rebuilt from the scatter table by `LoadRKnanoFW.java`, and exports per-function fingerprints with `ExportFunctions.java`.
2. `tools/match_functions.py` pairs functions across builds by fingerprint, first within the same block, then across blocks.
3. `DecompileFunctions.java` produces a full decompile of both builds (`work/functions/full_*.c`, ~164k lines each). `tools/diff_decomp.py` diffs them after normalising names.
4. `tools/gpio_calls.py` lists every literal GPIO operation per pin.

## Numbers
- 2771 (8G) vs 2757 (512M) functions.
- **2666 have identical logic**; 27 have the same shape with different constants (mostly relocated string/resource IDs).
- 78 / 64 functions (~33 KB) are build-specific.
- All ten core-B codec images (segments 73–82) are **byte-identical**, so audio decoding is the same.
- Fonts are identical. Resource 2 differs.

## GPIO: the boards are almost the same electrically
Rockchip DW GPIO accessors (same in both builds, with different addresses):

| Function | 8G address | 512M address | Register |
|---|---|---|---|
| iomux(port,pin,func) | 0x03052d70 | 0x03052d70 | GRF write-mask |
| dir(port,pin,out) | 0x03056a94 | 0x03056a60 | DDR +0x04 |
| write(port,pin,level) | 0x03056ace | 0x03056a9a | DR +0x00 |
| read(port,pin) | 0x03056aee | 0x03056aba | EXT +0x50 |

About 50 pins are used. Usage is identical except:

| Pin | 8 GB | 512 MB |
|---|---|---|
| **GPIO0.14** | peripheral mux only (func 1/2) | also driven as a **GPIO output** (high at boot, toggled in the USB-mode handler) |
| **GPIO2.1** | output low (8 sites) | output, controlled together with GPIO0.14 (14 sites) |

Both pins are switched in the USB-connect handler (string IDs 0xDE/0xDF = USB DAC vs data). The pattern depends on the `0x66666666` board ID (GPIO2.9 strap low) and stock's "UAC charging" setting at state+0x35D (decoded 2026-10-15 from MINIV390 disassembly of every write site):
- **GPIO0.14 = charge enable, 1 = charge** [inferred, strong]. High from reset, set by the charge screen, low at power-off.
- **GPIO2.1 = a VBUS → system supply path, 1 = run from VBUS** [inferred, medium].
- Setting values (non-0x666 board, DAC chosen): **0 (default) "Turn off charging power supply": both 0**; 1 "Power only, no charging": GPIO0.14 = 0, GPIO2.1 = 1 below 4000 mV battery, 0 above; 2 "Turn on charging power supply": GPIO0.14 = 1, GPIO2.1 = 0 (the normal state).
- On the 0x666 board both stay 1 everywhere and stock offers no DAC choice there.
- ReChord: `board_usb_supply()` and `dac_charge=off|on` (default off); test `tests/cfw/m14_dac_supply.sh`.

All of these are shared: the panel 8080 bus (GPIO0.16–0.23, 0.24/0.25), keys (GPIO0.11/0.13 reads + ADC), power (GPIO2.5/2.6), mute (GPIO2.0), backlight (GPIO2.4) and panel reset (GPIO2.15).

## Module-level differences
- **Boot/system init (`Reset_Handler`, 930 B → 1352 B):** the same sequence, plus GPIO0.14 high, GPIO2.1 handling, and a final GPIO2.1 decision keyed on `0x66666666`. The RTC default date is set to 2026-05-01 12:00 if invalid (both builds).
- **Updater (seg08):** looks for `ECHOMINI.IMG`/`HIFIECnn` on 8G and `MINIMINI.IMG`/`MINIVnnn` on 512M.
- **8G seg06** is a **Chinese lunar-calendar** module (1901–2100 tables, 12/10 stem-branch cycles, leap months, digit drawing). It has no counterpart in 512M.
  - The normal clock and date exist on both. The user confirmed this on their 512 MB unit (2026-10-14), and the date-setting code in seg32 is present in both builds.
  - 512M reuses the seg06 slot for new code (GPIO0.14 plus power/USB).
- **seg21:** the 512M build references `UNICODEXDIC` (a dictionary/font index resource).
- **seg09, seg32, seg01, seg11, seg25–31:** changed. seg32 is a large settings/UI module; seg09 is the USB-connect handler. *To do:* go through `work/functions/decomp_8G_vs_512M.diff`.

## Storage
Both builds link both the eMMC and SFC drivers. The storage-related functions show no build-specific differences in this pass, which fits the 512 MB board also using eMMC (a small part) and detecting it at runtime. *To confirm:* read the IDB "NANO" system size. That needs the device, so it waits for the user's OK.

## What this means for porting
The 8 GB-board knowledge (ReChord, RECHO) mostly transfers. Two areas need care:
1. GPIO0.14 / GPIO2.1. Mirror stock's 512M behaviour exactly; never treat GPIO0.14 as its 8G peripheral function.
2. Image naming and the layout constants (`MINIVnnn.IMG`, IDB/system size).

## Bluetooth (both boards)
- BT is a separate **JieLi** module. It sits **after the DACs**: it re-digitises the analog output and sends SBC. The firmware cannot improve that path, so it is a dead end for audio quality (community finding, 2026-10-14).
- Control is a simple text protocol (about 4 commands) over **UART at 115200 8N1**: UART config 3, TX GPIO2.16 func1, RX GPIO2.17. It is opened by `FUN_030538da` (512M) with the RX interrupt enabled. GPIO2.16/2.17 are also driven low as GPIOs in many places, probably to power the module down or hold it in reset.
- `uart_init` (`FUN_03056f16`) is a pin-mux table. Its config 6 uses GPIO0.14 func1, which is why the 8G build only ever "muxes" GPIO0.14: an unused table entry. **On 512M, GPIO0.14 is a plain GPIO output**, toggled with GPIO2.1 in the USB-mode handler.

## Goal: USB audio out (UAC host) to USB-C earphones
The SoC already does USB device mode (MSC + UAC "USB DAC"). The DW OTG core can in principle act as a host. Open questions:
1. Does the board supply **VBUS 5 V out** on USB-C, through a boost or OTG switch? Check whether GPIO0.14 / GPIO2.1 on 512M is a USB power/path control.
2. CC / ID detection for host role.
3. RAM budget for a minimal host stack (enumeration + isochronous OUT to one UAC1/UAC2 endpoint).
4. Decode on core B → I2S today; route PCM to USB isochronous instead.

## First 8 GB hardware run (2026-11-22): storage layout not found
`HIFIEC90.IMG` (BOARD=8g), installed through the stock 4.0.0 updater, booted on an 8 GB unit
(Samsung eMMC, 7456 MB, 15269888 sectors), recognised the board, then stopped at the boot-ROM
recovery screen: `7456M lay -1`. `emmc_layout()` (`drivers/emmc_rknanoc.c`) finds no IDB at LBA
`0x40 + n*0x400` (magic `0xFCDC8C3B`, "NANO" at +0x20A) on this eMMC, so Internal is never mounted
and the updater and Recovery cannot run.

A rockusb read of the first 128 MB (`rl 0 0x40000` through the boot ROM's loader) showed:
- the firmware area from LBA 0: slot 1 at 0, **slot 2 at 0x11000** (as on 512M), each the 5 MB
  envelope (fw size `0x450000` at +0x1F4, "RKnanoFW" at +0x1F8), both byte-identical to the
  flashed image;
- **no IDB magic anywhere** in it, and no FAT/exFAT/MBR (the user volume is further on).

The rockusb view starts at physical 0x2000 (loader LBA 0x22000 holds the "SAVE" block noted at
physical 0x24000), so the IDB at physical 0x40 is outside the dump. Everything else matches the
unit the IDB was read from in October (7456 MiB, area 0x22000, slot 2 at 0x11000, SAVE at
0x24000, volume at 0x88000); that unit had been through RECHO; this one is factory stock 4.0.0.

### Cause and fix (2026-11-23): reads failed after a half-done bus width switch
Two builds narrowed it down, each written to both slots from an Ubuntu live USB and showing what it
read on the recovery screen:
1. The firmware area from the slots when no IDB is recognised (`drivers/emmc_layout.h`): still
   `lay -1`.
2. PARTITION_CONFIG (EXT_CSD 179) on screen with the words read at 0x40 and at 0x2000:
   `7456M lay -1 p0 eeeeeeee eeeeeeee`. The user area was selected (p0), and **both reads
   failed** (`eeeeeeee`: "not read"); the EXT_CSD read during init had worked, on 1 bit.

`set_width()` switches the card to 8 (then 4) bits after a bus test and keeps the width only if
sector 0x40 reads back the same; otherwise it sends CMD6 back to 1 bit **without checking it**.
On this unit the wide read fails, the card is left out of `tran` and refuses that CMD6, and stays
on the wide bus while the host is on 1 bit: every later read fails, and init still returns 0. The
October unit's wide bus passed, so this path never ran there.

**Fix (8 GB build):** after the width is chosen, `emmc_init()` reads sector 0x40; if that fails it
runs again from CMD0 (which puts the card back on 1 bit) and stays on 1 bit. The slot fallback
stays (`drivers/tests/test_emmc_layout.c`: both boards; `EMMC_DUMP=` this unit's dump finds area
0x22000 and volume 0x88000). On a failed layout the 8 GB recovery screen shows
`<size> lay <r> w<width, +0x100 = init redone on 1 bit> <word at 0x40> <word at 0x2000+0x1F8>`.

**Verified on this unit (2026-11-23):** boots to the UI (first frame 232 ms), ECHOLOG
`emmc: eMMC, 1-bit, 15269888 sectors`, Internal mounted, the library indexed, USB disk works,
an APE track played, storage `errors 0`.

### The wide bus (2026-11-23): the bus test's own bug
A build that logged each width try (`emmc: widths` in ECHOLOG; `g_emmc_wd`, drivers/emmc_rknanoc.c)
showed 8-bit failing at the bus test with RINTSTS 0x801c (end-bit error on the CMD19 write), then
4-bit failing at its first plain read with a response timeout (0x104), the same at 12 MHz. CMD19
(BUSTEST_W) has no CRC status from the card (JEDEC), which the DW-MMC reports as an end-bit error;
the driver took that as a failed test and never sent CMD14, so the card stayed in its bus-test state
and answered nothing else. Stock ignores CMD19's result and always reads the pattern back
(HIFIEC40 0x39780). With that, the test reads `aa 55` back, SWITCH puts the card on 8 bits
(R1 0x0800, tran) and sector 0x40 reads the same as at 1 bit: the unit runs on the 8-bit bus at
24 MHz. USB disk: 100 MB written at 6.1 MB/s, 150 MB read at 8.1 MB/s (1 bit: about 0.8 MB/s
written); the eMMC is up at 0.074 s instead of 0.119 s. The width tries stay logged, SWITCH_ERROR is
checked as stock does, and a failed wide bus still ends on 1 bit (a second try at 12 MHz first).

Recovery, as used here: `rkdeveloptool wl 0 HIFIEC40.IMG` (docs/DEBRICK-LINUX.md) from an Ubuntu
live USB booted with `toram` (without it the live system lost its own USB stick); test builds
written with `wl 0` and `wl 0x11000` and read back. WSL + usbipd cannot attach the 2207:262d
loader ("Device in error state").
