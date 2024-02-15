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

#ifndef CALLBACKS_H
#define CALLBACKS_H

#include <stddef.h>
#include <stdbool.h>

#include "uassert.h"

#define N_CALLBACKS 5

extern void *pressure_callbacks[N_CALLBACKS];
extern void *mass_callbacks[N_CALLBACKS];
extern void *flow_callbacks[N_CALLBACKS];
extern void *temperature_callbacks[N_CALLBACKS];
extern void *tick_callbacks[N_CALLBACKS];
extern void *turn_callbacks[N_CALLBACKS];
extern void *click_callbacks[N_CALLBACKS];
extern void *panel_callbacks[N_CALLBACKS];

#define add_callback(F, CALLBACKS) {                            \
        int _i;                                                 \
                                                                \
        for (_i = 0; _i < N_CALLBACKS && CALLBACKS[_i]; _i++);  \
                                                                \
        uassert(_i < N_CALLBACKS);                              \
        CALLBACKS[_i] = F;                                      \
    }

#define RUN_CALLBACKS(CALLBACKS, T, ...) {              \
        for (int _i = 0; _i < N_CALLBACKS; _i++) {      \
            if (!CALLBACKS[_i]) {                       \
                continue;                               \
            }                                           \
                                                        \
            if (((T)CALLBACKS[_i])(__VA_ARGS__)) {      \
                CALLBACKS[_i] = NULL;                   \
            }                                           \
        }                                               \
    }

#endif
