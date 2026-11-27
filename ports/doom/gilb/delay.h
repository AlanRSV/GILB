/* delay.h -- GILB: waits through the program API (the MG24 port polled a timer). */
#ifndef SRC_DELAY_H_
#define SRC_DELAY_H_
#include "gilb_board.h"
static inline void delay(uint32_t milliseconds) { gilb->sleep_ms(milliseconds); }
#endif
