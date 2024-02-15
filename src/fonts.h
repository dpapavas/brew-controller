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

#ifndef FONTS_H
#define FONTS_H

#include <stdint.h>

__attribute__ ((section (".flashdata")))
extern uint8_t bold_10_metrics[][6];

__attribute__ ((section (".flashdata")))
extern uint8_t bold_10_pixels[];

__attribute__ ((section (".flashdata")))
extern uint8_t bold_14_metrics[][6];

__attribute__ ((section (".flashdata")))
extern uint8_t bold_18_metrics[][6];

__attribute__ ((section (".flashdata")))
extern uint8_t bold_14_pixels[];

__attribute__ ((section (".flashdata")))
extern uint8_t bold_18_pixels[];

struct font {
    uint8_t width;
    uint8_t base;
    uint8_t advance[2];
    uint8_t (*metrics)[6];
    uint8_t *pixels;
    struct font_kerning {
        char character;
        int8_t left, right;
    } kerning[];
};

extern const struct font bold_10;
extern const struct font bold_14;
extern const struct font bold_18;

#endif
