#include <stdarg.h>
#include <math.h>

#include "mk20dx.h"
#include "fonts.h"
#include "time.h"
#include "uassert.h"

#define delay_pixel() delay_us(5)

#define TRANSMIT(...) {                                                 \
        const uint8_t _v[] = {__VA_ARGS__};                             \
        const size_t _n = sizeof(_v);                                   \
        for (size_t _i = 0; _i < _n; _i++) {                            \
            SPI0_SR |= SPI_SR_TFFF;                                     \
            WAIT_WHILE(!(SPI0_SR & SPI_SR_TFFF), 100);                  \
            SPI0_PUSHR = (                                              \
                SPI_PUSHR_PCS(4) | SPI_PUSHR_CTAS(0)                    \
                | _v[_i] | ((_i < _n - 1) ? SPI_PUSHR_CONT : 0));       \
        }                                                               \
    }

#define COMMANDS(...)  {                                \
        if (GPIOB_PDOR & PT(0)) {                       \
            WAIT_WHILE(!(GPIOC_PDIR & PT(0)), 100);     \
            GPIOB_PCOR = PT(0);                         \
        }                                               \
        TRANSMIT(__VA_ARGS__);                          \
    }

#define DATA(...)  {                                    \
        if (!(GPIOB_PDOR & PT(0))) {                    \
            WAIT_WHILE(!(GPIOC_PDIR & PT(0)), 100);     \
            GPIOB_PSOR = PT(0);                         \
        }                                               \
        TRANSMIT(__VA_ARGS__);                          \
    }

#define COMMAND(X, ...)  {                              \
        COMMANDS(X);                                    \
        if (sizeof((uint8_t []){__VA_ARGS__}) > 0) {    \
            DATA(__VA_ARGS__);                          \
        }                                               \
    }

static inline const uint8_t *choose_background_color(int x, int y)
{
#define COLOR(B, G, R) {                        \
        (B & 0xf8) | (G >> 5),                  \
        (((G >> 2) & 0x7) << 5) | (R >> 3)}

    static const uint8_t palette[][2] = {
        COLOR(105, 90, 70),
        COLOR(70, 60, 47)
    };

    return palette[(y / 32 + x / 64) % 2];

#undef COLOR
}

void reset_display(void)
{
    /* 8-bit frame, 48 Mhz bus clock / 2 / 6 = 4 Mhz SCK.  */

    SPI0_MCR |= SPI_MCR_PCSIS(4);
    SPI0_CTAR0 = SPI_CTAR_FMSZ(7) | SPI_CTAR_PBR(0) | SPI_CTAR_BR(2);

    /* Configure DC and RST as outputs. */

    SIM_SCGC5 |= SIM_SCGC5_PORTB | SIM_SCGC5_PORTC;
    PORTB_PCR0 = PORT_PCR_MUX(1);                /* D/C */
    PORTB_PCR1 = PORT_PCR_MUX(1) | PORT_PCR_SRE; /* RST */
    GPIOB_PDDR |= (PT(0) | PT(1));
    PORTC_PCR0 = PORT_PCR_MUX(2) | PORT_PCR_DSE; /* CS4 */

    GPIOB_PSOR = PT(1);
    delay_ms(100);
    GPIOB_PCOR = PT(1);
    delay_ms(100);
    GPIOB_PSOR = PT(1);
    delay_ms(100);

    COMMAND(0xae);
    COMMAND(0xfd, 0x12);        /* Unlock MCU interface. */
    COMMAND(0xfd, 0xb1);        /* Unlock commands. */

    COMMAND(0xa0, 0x21);        /* Set re-map and format. */
    COMMAND(0xa2, 0);           /* No scroll. */
    COMMAND(0xb1, 0x43);        /* Set pre-charge phases. */
    COMMAND(0xb2, 0xa4, 0x0, 0x0);   /* Enhance display performance. */
    COMMAND(0xb3, 0xf1);             /* Set oscillator. */
    COMMAND(0xc1, 0xc8, 0x80, 0xc0); /* Set contrasts. */
    COMMAND(0xc7, 0x0f);

    COMMANDS(0xa6, 0xaf);

  error:
}

void clear_display(void)
{
    COMMAND(0x75, 0, 127);
    COMMAND(0x15, 0, 127);
    COMMAND(0x5c);

    for (int j = 0; j < 128; j++) {
        for (int i = 0; i < 128; i++) {
            const uint8_t *c = choose_background_color(i, j);

            DATA(c[0], c[1]);
            delay_pixel();
        }
    }

  error:
}

#define PUT_PIXEL(A, B)                         \
    DATA(A, B);                                 \
    delay_pixel();
#define CLEAR_PIXEL() PUT_PIXEL(bg[0], bg[1])

static int clear_to(uint8_t x_to, uint8_t x, uint8_t y, const struct font *font)
{
    if (x > 127 || x_to < x) {
        return x;
    }

    if (x_to / 64 > x / 64) {
        x_to = x - (x % 64) + 63;
    }

    const uint8_t y_0 = y - font->base;
    const uint8_t delta_y = font->advance[1];
    const uint8_t *bg = choose_background_color(x, y);

    COMMAND(0x75, x, x_to);
    COMMAND(0x15, y_0, y_0 + delta_y - 1);
    COMMAND(0x5c);

    for (int j = 0; j < delta_y; j++) {
        for (int i = 0; i < x_to - x + 1; i++) {
            CLEAR_PIXEL();
        }
    }

  error:
    return x_to;
}

int display_c(uint8_t c, uint8_t x, uint8_t y, const uint8_t *fg,
              const struct font *font)
{
    const struct font_kerning *kerning;
    for (kerning = font->kerning;
         kerning->character && kerning->character != c;
         kerning++);

    const uint8_t *metrics = font->metrics[c - 32];
    const uint8_t w = metrics[2], h = metrics[3];
    const uint8_t w_off = metrics[4] + kerning->left, h_off = metrics[5];
    const uint8_t w_0 = font->width;
    const uint8_t y_0 = y - font->base;
    const uint8_t delta_x = font->advance[0] + kerning->right;
    const uint8_t delta_y = font->advance[1];
    const uint8_t *bg = choose_background_color(x, y);

    const uint8_t x_1 = x + delta_x - 1;

    if (x_1 / 64 > x / 64) {
        return 0;
    }

    COMMAND(0x75, x, x_1);
    COMMAND(0x15, y_0, y_0 + delta_y - 1);
    COMMAND(0x5c);

    for (int j = 0; j < h_off; j++) {
        for (int i = 0; i < delta_x; i++) {
            CLEAR_PIXEL();
        }
    }

    for (int j = 0; j < h; j++) {
        for (int i = 0; i < w_off; i++) {
            CLEAR_PIXEL();
        }

        for (int i = 0; i < w; i++) {
            const int k = ((metrics[1] + j) * w_0 + metrics[0] + i);
            const uint8_t v = (font->pixels[k] >> 2);

            PUT_PIXEL(((((v >> 1) << 3) | (v >> 3)) & fg[0]) | bg[0],
                      ((((v & 0x7) << 5) | (v >> 1)) & fg[1]) | bg[1]);
        }

        for (int i = 0; i < delta_x - w - w_off; i++) {
            CLEAR_PIXEL();
        }
    }

    for (int j = 0; j < delta_y - h - h_off; j++) {
        for (int i = 0; i < delta_x; i++) {
            CLEAR_PIXEL();
        }
    }

  error:
    return delta_x;
}

#undef PUT_PIXEL
#undef CLEAR_PIXEL

static int display_i_inner(int i, int w, uint8_t x, uint8_t y,
                           const uint8_t *fg,
                           const struct font *font)
{
    if (i == 0) {
        int n = 0;

        for (int j = 0;
             j < w;
             j++, n += display_c(' ', x + n, y, fg, font));

        return n;
    } else {
        int n = display_i_inner(i / 10, w - 1, x, y, fg, font);

        return n + display_c('0' + (i % 10), x + n, y, fg, font);
    }
}

int display_i(int i, int w,
              uint8_t x, uint8_t y, const uint8_t *fg,
              const struct font *font)
{
    int n = 0;

    if (i == 0) {
        n += display_i_inner(i, w - 1, x + n, y, fg, font);
        return n + display_c('0', x + n, y, fg, font);
    }

    if (i < 0) {
        n = display_c('-', x, y, fg, font);
        i = -i;
    }

    return n + display_i_inner(i, w, x + n, y, fg, font);
}

static int display_d_inner(double f, int w, uint8_t x, uint8_t y,
                           const uint8_t *fg,
                           const struct font *font)
{
    if (w == 0) {
        return 0;
    } else {
        int n;

        f *= 10;
        n = display_c('0' + ((int)f % 10), x, y, fg, font);

        return n + display_d_inner(f, w - 1, x + n, y, fg, font);
    }
}

int display_d(double d, int w, int p, uint8_t x, uint8_t y,
              const uint8_t *fg,
              const struct font *font)
{
    double i, f;
    int n;

    f = modf(d, &i);

    n = display_i((int)i, w - p - (p > 0), x, y, fg, font);

    if (p == 0) {
        return n;
    }

    n += display_c('.', x + n, y, fg, font);

    return n + display_d_inner(fabs(f), p, x + n, y, fg, font);
}

void display(const char *s, ...)
{
    const struct font *font = &bold_10;
    uint8_t fg[2] = {0xff, 0xff};
    uint8_t x_set = 0, x = 0, y = 0;

    va_list ap;
    va_start(ap, s);

    for (const char *c = s; *c != '\0'; c++) {
        if (*c == 1) {
            font = &bold_10;
            continue;
        }

        if (*c == 2) {
            font = &bold_14;
            continue;
        }

        if (*c == 3) {
            font = &bold_18;
            continue;
        }

        if (*c == '\a') {
            x_set = x = *(uint8_t *)(++c);
            y = *(uint8_t *)(++c);
            continue;
        }

        if (*c == '\n') {
            x = x_set;
            y += font->advance[1] + 1;
            continue;
        }

        if (*c == '\f') {
            for (int i = 0; i < 2; i++) {
                fg[i] = *(uint8_t *)(++c);
            }
            continue;
        }

        if (*c == '\v') {
            x = clear_to(x - (x % 64) + 63, x, y, font);
            continue;
        }

        if (*c == '%') {
            uint8_t d = *(uint8_t *)(++c);
            int w_i = 0, w_f = 1;

            if (d >= '0' && d <= '9') {
                w_i = d - '0';
                d = *(uint8_t *)(++c);
            }

            if (d == '.') {
                d = *(uint8_t *)(++c);
            }

            if (d >= '0' && d <= '9') {
                w_f = d - '0';
                d = *(uint8_t *)(++c);
            }

            switch(d) {
            case '%': break;
            case 'd':
                x += display_i(va_arg(ap, int), w_i, x, y, fg, font);
                continue;
            case 'f':
                x += display_d(
                    va_arg(ap, double), w_i, w_f, x, y, fg, font);
                continue;
            }
        }

        x += display_c(*c, x, y, fg, font);
    }

    va_end(ap);
}
