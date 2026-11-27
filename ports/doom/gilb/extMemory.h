/*
 * extMemory.h -- the game's WAD and save data, read from files on the player's storage.
 *
 * The MG24 port (Nicola Wrachien, GPLv2 or later) kept the converted WAD on an SPI flash and read
 * it through this API: an "address" is EXT_FLASH_BASE + an offset, the first 4 bytes are the WAD's
 * length and WAD_ADDRESS (EXT_FLASH_BASE + 4) is the WAD's first byte. GILB keeps that address
 * space and serves it from files (gilb_wad.c):
 *   - EXT_FLASH_BASE + 0..3: the length of DOOM.WAD (GILB/APPS/DOOM.WAD, the converted WAD)
 *   - WAD_ADDRESS + n: byte n of it, through a read cache in RAM (the storage is slow per request)
 *   - the top of the 16 MiB space: the save game and the settings, each a file of its own
 * Reads finish before the call returns, so the "asynchronous" read is a copy and its wait is empty.
 */
#ifndef SRC_EXTMEMORY_H_
#define SRC_EXTMEMORY_H_
#include <stdint.h>
#include <string.h>
#include "gilb_board.h"

#define EXT_MEMORY_HEADER_SIZE      8   /* (kept: the game sizes its buffers with it) */
#define EXT_MEMORY_READ_ALIGN_SIZE  4
#define SPI_ADDRESS_MASK            0x00FFFFFFu
#define GILB_EXT_SIZE               0x01000000u   /* the address space: 16 MiB */

extern uint32_t currentSpiAddress;   /* offset of the next sequential read */

void extMemInit(void);
void gilb_ext_read(uint32_t offset, void *dest, uint32_t n);

static inline void extMemSetCurrentAddress(uint32_t address)
{
    currentSpiAddress = address & SPI_ADDRESS_MASK;
}

static inline void *extMemGetDataFromCurrentAddress(void *dest, unsigned int length)
{
    gilb_ext_read(currentSpiAddress, dest, length);
    currentSpiAddress += length;
    return dest;
}

static inline uint8_t extMemGetByteFromAddress(const void *addr)
{
    uint8_t b;

    extMemSetCurrentAddress((uint32_t)addr);
    extMemGetDataFromCurrentAddress(&b, 1);
    return b;
}

static inline short extMemFlashGetShortFromAddress(const void *addr)
{
    short s;

    extMemSetCurrentAddress((uint32_t)addr);
    extMemGetDataFromCurrentAddress(&s, 2);
    return s;
}

/* The data lands at dest + EXT_MEMORY_HEADER_SIZE + (address & 3), as on the MG24. */
static inline void *extMemStartAsynchDataRead(uint32_t address, void *dest, uint32_t cnt)
{
    uint8_t *d = (uint8_t *)dest + EXT_MEMORY_HEADER_SIZE + (address & 3u);

    gilb_ext_read(address & SPI_ADDRESS_MASK, d, cnt);
    return d;
}

static inline void extMemWaitAsynchDataRead(void) { }
static inline void extMemRestoreInterface(void) { }
static inline int extMemGetSize(void) { return (int)GILB_EXT_SIZE; }

/* Writes: only the save game's and the settings' records (g_game.c), each written whole. */
void extMemWrite(uint32_t address, void *buffer, uint32_t size);
static inline void extMemErase(uint32_t address, uint32_t size) { (void)address; (void)size; }
static inline void extMemProgram(uint32_t address, void *buffer, uint32_t size) { extMemWrite(address, buffer, size); }

#define SET_FLASH_MODE()
#endif
