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

#include "callbacks.h"

void *pressure_callbacks[N_CALLBACKS];
void *mass_callbacks[N_CALLBACKS];
void *flow_callbacks[N_CALLBACKS];
void *temperature_callbacks[N_CALLBACKS];
void *tick_callbacks[N_CALLBACKS];
void *turn_callbacks[N_CALLBACKS];
void *click_callbacks[N_CALLBACKS];
void *panel_callbacks[N_CALLBACKS];
