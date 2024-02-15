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

#ifndef _USB_H_
#define _USB_H_

#include <stdint.h>
#include <stdbool.h>

void reset_usb(void);
int is_usb_enumerated(void);
void await_usb_enumeration(void);
int write_usb(const char *s, int n, bool flush);
int interrupt_usb(uint8_t *buffer, size_t n);
void set_usb_data_in_callback(void (*new_callback)(uint8_t *data, size_t n));

int is_usb_dtr(void);
void await_usb_dtr(void);
int is_usb_rts(void);
void await_usb_rts(void);
int set_usb_serial_state(uint16_t state);

int uprintf(const char *format, ...);

#endif
