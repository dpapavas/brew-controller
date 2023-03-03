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
