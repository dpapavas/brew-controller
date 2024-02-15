/* Copyright (C) 2024 Papavasileiou Dimitris
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

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
