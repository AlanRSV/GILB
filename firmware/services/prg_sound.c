/*
 * prg_sound.c -- sound for programs (sdk/include/gilb_app.h API version 5, sound_*).
 *
 * A program gives a ring of PCM frames in its own memory (two int32 words each, L then R, the
 * sample in the top bits, as core B writes the PCM ring); this brings the DAC up at the program's
 * rate (32 kHz and up: dac_cs43131.c's table has 8 to 24 kHz too, but the CS43131 plays nothing
 * there, device 2026-11-26) and points the audio DMA at the ring in a
 * loop, block by block along an LLI chain kept here. The program writes ahead of sound_pos() as the
 * DMA moves on. The DMA, the I2S and the DAC are the player's own, the way audio.c's dma_start()
 * drives them; the music is stopped and core B held first (audio_pcm_free). When the program ends,
 * prg_sound_close() stops it all and usb_dac_rail_cut() has the next play bring the DAC up afresh
 * (its rate and state are this file's now, not audio.c's).
 *
 * The whole file is the prg overlay, its LLI chain too (firmware_ap.ld): no resident code or RAM.
 */
#include <stdint.h>
#include <string.h>

#include "audio/audio.h"
#include "drivers/dac_cs43131.h"
#include "drivers/usb_msc.h"
#include "services/log.h"

/* audio.c's registers and channel (the one the music uses) */
#define REG(a)      (*(volatile uint32_t *)(a))
#define DMA         0x60000000u
#define DMA_CH      5u
#define DMA_CHR(r)  REG(DMA + DMA_CH * 0x58u + (r))
#define DMA_CFGREG  REG(DMA + 0x398u)
#define DMA_CHEN    REG(DMA + 0x3A0u)
#define I2S1        0x40030000u
#define I2S_DMACR   REG(I2S1 + 0x10u)
#define I2S_TXDR    (I2S1 + 0x24u)
#define LLI_CTL_L   0x18909124u           /* M2P, 32-bit, src inc, burst 8, LLP src+dst */

#define MAX_BLOCKS 16u

static uint32_t s_lli[MAX_BLOCKS * 7u] __attribute__((aligned(8)));
static uint32_t s_ring, s_frames;     /* the program's ring: address, frames in all */
static uint32_t s_rate, s_bf, s_blocks;   /* (prg_sound_restart) */
static int s_on;

static void dma_off(void)
{
    I2S_DMACR = 0;
    DMA_CHEN = 1u << (DMA_CH + 8u);
    dac_i2s_stop();
}

/* 0, or < 0: -1 a bad ring, -2 the DAC did not come up at that rate. */
int prg_sound_open(uint32_t rate, int32_t *ring, uint32_t block_frames, uint32_t blocks)
{
    uint32_t a = (uint32_t)ring, bytes = block_frames * blocks * 8u;
    int r;

    if (rate < 32000u || blocks < 2u || blocks > MAX_BLOCKS || block_frames < 16u || block_frames > 4096u || (a & 7u) ||
        a < 0x03000000u || a + bytes > 0x030A0000u)  /* (core A's SRAM: what this DMA reads) */
        return -1;
    if (s_on)
        dma_off();
    audio_pcm_free();                /* (the music stopped, core B held, the DMA free) */
    if ((r = dac_init(rate)) != 0) {
        log_printf("programs: sound at %lu Hz: DAC %d", (unsigned long)rate, r);
        usb_dac_rail_cut();
        s_on = 0;
        return -2;
    }
    {
        uint8_t v = (uint8_t)(-audio_get_volume_db() * 2);   /* the player's volume (audio.c dac_volume) */

        for (int ch = 0; ch < 2; ch++) {
            dac_write_reg(ch, 0x090001u, v);
            dac_write_reg(ch, 0x090002u, v);
        }
    }
    memset(ring, 0, bytes);
    for (uint32_t i = 0; i < blocks; i++) {
        uint32_t *l = s_lli + i * 7u;

        l[0] = a + i * block_frames * 8u;
        l[1] = I2S_TXDR;
        l[2] = (uint32_t)(s_lli + ((i + 1u) % blocks) * 7u);
        l[3] = LLI_CTL_L;
        l[4] = block_frames * 2u;    /* words */
        l[5] = l[6] = 0;
    }
    s_ring = a;
    s_frames = block_frames * blocks;
    s_rate = rate;
    s_bf = block_frames;
    s_blocks = blocks;
    DMA_CFGREG = 1;
    DMA_CHR(0x00) = a;
    DMA_CHR(0x08) = I2S_TXDR;
    DMA_CHR(0x10) = (uint32_t)s_lli;   /* first LLI is fetched at enable */
    DMA_CHR(0x18) = LLI_CTL_L;
    DMA_CHR(0x1C) = block_frames * 2u;
    DMA_CHR(0x40) = 0x840u;
    DMA_CHR(0x44) = 0x1007u;          /* dest handshake 2 = I2S1 TX */
    DMA_CHEN = (1u << (DMA_CH + 8u)) | (1u << DMA_CH);
    I2S_DMACR = 0x110u;               /* TX DMA on, level 16 */
    dac_i2s_start();
    s_on = 1;
    log_printf("programs: sound on, %lu Hz, %lu x %lu frames", (unsigned long)rate, (unsigned long)blocks,
               (unsigned long)block_frames);
    return 0;
}

/* The frame the DMA reads now (0 .. blocks x block_frames - 1). */
uint32_t prg_sound_pos(void)
{
    uint32_t off;

    if (!s_on)
        return 0;
    off = DMA_CHR(0x00) - s_ring;
    return off / 8u % s_frames;
}

/* The USB serial port came up (serial_open: usb_dwc2_init's hw_on cuts the DAC's supply, mutes it and
 * switches the amp off, as for a USB host: device 2026-11-26, the DMA and the I2S running, nothing
 * heard): the DAC brought up again, as the USB DAC runs it, with USB on. The ring starts silent. */
void prg_sound_restart(void)
{
    if (s_on)
        (void)prg_sound_open(s_rate, (int32_t *)s_ring, s_bf, s_blocks);
}

void prg_sound_close(void)
{
    if (!s_on)
        return;
    dma_off();
    usb_dac_rail_cut();              /* (the next play brings the DAC up at its own rate) */
    s_on = 0;
    log_printf("programs: sound off");
}

int prg_sound_on(void)
{
    return s_on;
}
