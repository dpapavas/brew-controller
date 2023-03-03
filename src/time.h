#ifndef TIME_H
#define TIME_H

#include <inttypes.h>
#include "usb.h"

#ifdef TEENSY30
#define COUNTS_PER_US 48
#else
#define COUNTS_PER_US 72
#endif

void reset_time(void);
double get_time(void);
void delay_us(int32_t n);
#define delay_ms(N) delay_us(1000 * N)
#define DELAY_WHILE_US(COND, N, ACTION)                                 \
    {                                                                   \
        const uint32_t _r = SYST_RVR + 1, _c = SYST_CVR + _r;           \
        while (COND) {                                                  \
            if (((_c - SYST_CVR) % _r) / COUNTS_PER_US > N) {           \
                if (COND) ACTION;                                       \
                break;                                                  \
            }                                                           \
        }                                                               \
    }

#define WAIT_WHILE(COND, N, ...)                                \
    DELAY_WHILE_US(                                             \
        COND, N, {                                              \
            uprintf(                                            \
                "timeout after %d us (" __FILE__ ":%d)\n",      \
                N, __LINE__);                                   \
            __VA_ARGS__;                                        \
            goto error;                                         \
        })

#endif
