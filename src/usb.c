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

#include <math.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mk20dx.h"
#include "uassert.h"
#include "usb_private.h"

#define NOTIFICATION_SERIALSTATE 0x20a1

#define LINE_STATE_DTR (1 << 0)
#define LINE_STATE_RTS (1 << 1)

#define SERIAL_STATE_DCD (1 << 0)
#define SERIAL_STATE_DSR (1 << 1)

__attribute__ ((section(".usbdata.bdt")))
static struct bdtentry bdt[ENDPOINTS_N * 4];

static struct {
    __attribute__ ((aligned(4)))
    uint8_t setup[8];

    __attribute__ ((aligned(4)))
    uint8_t notification[2][NOTIFICATION_BUFFER_SIZE];

    __attribute__ ((aligned(4)))
    uint8_t serial[4][DATA_BUFFER_SIZE];
}  buffers;

static volatile struct {
    uint8_t *data;
    uint16_t length;
} pending, transmit;

static uint8_t oddbits, address;
static volatile uint8_t configuration;
static volatile uint8_t line_state;

static enum {
    PHASE_SETUP,
    PHASE_DATA_IN,
    PHASE_DATA_OUT,
    PHASE_STATUS_IN,
    PHASE_STATUS_OUT,
    PHASE_COMPLETE,
} phase;

#define DATA_IN_BUFFER_SIZE 1024
__attribute__ ((section(".flexram.upper")))
static uint8_t data_in_buffer[DATA_IN_BUFFER_SIZE];
static size_t data_in_count = DATA_IN_BUFFER_SIZE;
static bool clear_data_in = true;

static void (*data_in_callback)(uint8_t *data, size_t n);

static inline int process_setup_packet(uint16_t request, uint16_t value,
                                       uint16_t index, uint16_t length)
{
    switch (request) {
    case SETUP_DEVICE_GET_DESCRIPTOR:
        /* GET_DESCRIPTOR request. */

        switch (DESCRIPTOR_TYPE(value)) {
        case DESCRIPTOR_TYPE_DEVICE:
            /* GET_DESCRIPTOR for DEVICE. */

            pending.data = device_descriptor;
            pending.length = 18;

            return PHASE_DATA_IN;

        case DESCRIPTOR_TYPE_DEVICE_QUALIFIER:
            /* GET_DESCRIPTOR for DEVICE QUALIFIER. */

            USB0_ENDPT(CONTROL_ENDPOINT) |= USB_ENDPT_EPSTALL;

            return PHASE_COMPLETE;

        case DESCRIPTOR_TYPE_DEVICE_CONFIGURATION:
            /* GET_DESCRIPTOR for DEVICE CONFIGURATION. */

            pending.data = configuration_descriptor;
            pending.length = sizeof(configuration_descriptor);

            return PHASE_DATA_IN;
        }

        /* We've received a request for an unimplemented device
         * descriptor, so we just stall. */

        USB0_ENDPT(CONTROL_ENDPOINT) |= USB_ENDPT_EPSTALL;

        return PHASE_COMPLETE;

    case SETUP_DEVICE_GET_CONFIGURATION:
        /* GET_CONFIGURATION request. */

        pending.data = (uint8_t *)&configuration;
        pending.length = sizeof(configuration);

        return PHASE_DATA_IN;

    case SETUP_DEVICE_SET_CONFIGURATION:
        /* SET_CONFIGURATION request. */

        pending.data = NULL;
        pending.length = 0;

        if (value != 1) {
            USB0_ENDPT(CONTROL_ENDPOINT) |= USB_ENDPT_EPSTALL;

            return PHASE_COMPLETE;
        } else {
            /* Initialize endpoint 1. */

            USB0_ENDPT(NOTIFICATION_ENDPOINT) = (USB_ENDPT_EPTXEN |
                                                 USB_ENDPT_EPHSHK);

            for (int i = 0 ; i < 2 ; i += 1) {
                struct bdtentry *t = bdt_entry(NOTIFICATION_ENDPOINT, 1, i);

                t->buffer = buffers.notification[i];
                t->desc = bdt_descriptor(0, 0);
            }

            reset_oddbit(NOTIFICATION_ENDPOINT, 0);
            reset_oddbit(NOTIFICATION_ENDPOINT, 1);

            /* Initialize endpoint 2. */

            USB0_ENDPT(DATA_ENDPOINT) = (USB_ENDPT_EPRXEN | USB_ENDPT_EPTXEN |
                                         USB_ENDPT_EPHSHK);

            /* Initialize the buffer pointers. */

            reset_oddbit(DATA_ENDPOINT, 0);
            reset_oddbit(DATA_ENDPOINT, 1);

            for (int i = 0 ; i < 2 ; i += 1) {
                struct bdtentry *r = bdt_entry(DATA_ENDPOINT, 0, i);
                struct bdtentry *t = bdt_entry(DATA_ENDPOINT, 1, i);

                r->buffer = buffers.serial[i];
                t->buffer = buffers.serial[2 + i];

                /* Prime the first input BDT entry. */

                r->desc = bdt_descriptor(DATA_BUFFER_SIZE, i);

                if (i == 0) {
                    r->desc |= BDT_DESC_OWN;
                    toggle_oddbit(DATA_ENDPOINT, 0);
                }

                t->desc = bdt_descriptor(0, 0);
            }

            transmit.data = NULL;
            transmit.length = DATA_BUFFER_SIZE;

            configuration = (uint8_t)value;
        }

        return PHASE_DATA_OUT;

    case SETUP_DEVICE_SET_ADDRESS:
        /* SET_ADDRESS request. */

        pending.data = NULL;
        pending.length = 0;

        address = (uint8_t)value;

        return PHASE_DATA_OUT;

    case SETUP_SET_CONTROL_LINE_STATE:
        /* SET_CONTROL_LINE_STATE request. */

        pending.data = NULL;
        pending.length = 0;

        line_state = (uint8_t)value;

        return PHASE_DATA_OUT;
    }

    /* We've received an unimplemented request, so we
     * just stall. */

    USB0_ENDPT(CONTROL_ENDPOINT) |= USB_ENDPT_EPSTALL;

    return PHASE_COMPLETE;
}

static void handle_data_transfer(uint8_t status)
{
    const struct bdtentry *entry = &(bdt[status >> 2]);
    const int pid = BDT_DESC_PID(entry->desc);

    if (pid == BDT_PID_OUT) {
        struct bdtentry *in;

        uassert(bdt_entry(DATA_ENDPOINT, 0,
                          !oddbit(DATA_ENDPOINT, 0)) == entry);

        if (data_in_callback) {
            if (clear_data_in) {
                memset(data_in_buffer, 0, data_in_count);
                data_in_count = 0;
                clear_data_in = false;
            }

            size_t n = BDT_DESC_BYTE_COUNT(entry->desc);
            bool p = false;

            /* Ignore EOL characters. */

            {
                uint8_t *c = entry->buffer + n - 1;

                while (n > 0 && (*c == '\n' || *c == '\r')) {
                    c--;
                    n--;
                    p = true;
                }
            }

            /* Check for buffer overflew (allowing for one byte of
             * null-termination and discard this line entirely. */

            if (data_in_count + n >= DATA_IN_BUFFER_SIZE) {
                data_in_count = DATA_IN_BUFFER_SIZE;
            } else if (n > 0) {
                memcpy(data_in_buffer + data_in_count, entry->buffer, n);
                data_in_count += n;
            }

            if (p && data_in_count > 0) {
                if (data_in_count < DATA_IN_BUFFER_SIZE) {
                    data_in_callback(data_in_buffer, data_in_count);
                }

                clear_data_in = true;
            }
        }

        in = bdt_entry(DATA_ENDPOINT, 0, oddbit(DATA_ENDPOINT, 0));
        uassert(!(in->desc & BDT_DESC_OWN));

        in->desc = bdt_descriptor(DATA_BUFFER_SIZE, oddbit(DATA_ENDPOINT, 0));
        in->desc |= BDT_DESC_OWN;

        toggle_oddbit(DATA_ENDPOINT, 0);
    }
}

static void handle_control_transfer(uint8_t status)
{
    const struct bdtentry *entry = &(bdt[status >> 2]);
    const int pid = BDT_DESC_PID(entry->desc);

    uint16_t *buffer, length, value, request, index;
    int n;

    /* Some notes:

      * Double-buffering of BDT entries works on the level of
        *endpoints* (tx and rx channels are considered separate
        endpoints).

      * Each time we receieve a TOKDNE interrupt a token has already
        been processed.  In order for that to happen we need to
        prepare and release a BDT entry, in the correct (odd/even)
        bank and with the correct data toggle bit (DATA0/1), for the
        next token during each service routine call.

      * Each control transaction consists of up to 3 stages: SETUP, DATA
        and STATUS.

      * Each stage has a data packet.

      * The SETUP data packet is always DATA0 and the request determines
        if the DATA stage exists and what direction it has.

      * The DATA stage data packets alternate between DATA0 and DATA1
        and must transfer the amount of data specified in the SETUP
        phase.

      * The STATUS phase data packet is always of the opposite direction
        with respect to the preceding stage and always DATA1. */

    if (phase == PHASE_SETUP && pid == BDT_PID_SETUP) {
        /* Read the payload and release the buffer. */

        buffer = (uint16_t *)entry->buffer;
        request = buffer[0];
        value = buffer[1];
        index = buffer[2];
        length = buffer[3];

        phase = process_setup_packet(request, value, index, length);

        /* Make sure we don't return more than the host asked
         * for. */

        if (phase == PHASE_DATA_IN && length < pending.length) {
            pending.length = length;
        }
    }

    if (phase == PHASE_DATA_IN &&
        (pid == BDT_PID_IN || pid == BDT_PID_SETUP)) {

        /* Determine whether there's anything else to send
         * and prepare the buffer, otherwise transition to
         * the status phase. */

        n = pending.length < CONTROL_BUFFER_SIZE ?
            pending.length : CONTROL_BUFFER_SIZE;

        if (n > 0) {
            const int data1 = !(entry->desc & BDT_DESC_DATA1);
            struct bdtentry *out =
                bdt_entry(CONTROL_ENDPOINT, 1, oddbit(CONTROL_ENDPOINT, 1));

            uassert(!(out->desc & BDT_DESC_OWN));

            out->buffer = (void *)pending.data;
            out->desc = bdt_descriptor(n, data1);
            out->desc |= BDT_DESC_OWN;

            pending.data += n;
            pending.length -= n;

            toggle_oddbit(CONTROL_ENDPOINT, 1);
        } else {
            struct bdtentry *in =
                bdt_entry(CONTROL_ENDPOINT, 0, oddbit(CONTROL_ENDPOINT, 0));

            /* Prepare to receive a zero-length DATA1 packet to
             * aknowledge proper reception during the preceding
             * IN phase. */

            uassert(!(in->desc & BDT_DESC_OWN));

            in->buffer = NULL;
            in->desc = bdt_descriptor(0, 1);
            in->desc |= BDT_DESC_OWN;

            toggle_oddbit(CONTROL_ENDPOINT, 0);
            phase = PHASE_STATUS_IN;
        }
    }

    if (phase == PHASE_DATA_OUT &&
        (pid == BDT_PID_OUT || pid == BDT_PID_SETUP)) {

        /* Determine whether there's anything else to receive and
         * prepare the buffer, otherwise transition to the status
         * phase. */

        n = pending.length < CONTROL_BUFFER_SIZE ?
            pending.length : CONTROL_BUFFER_SIZE;

        if (n > 0) {
            const int data1 = !(entry->desc & BDT_DESC_DATA1);
            struct bdtentry *in =
                bdt_entry(CONTROL_ENDPOINT, 0, oddbit(CONTROL_ENDPOINT, 0));

            uassert(!(in->desc & BDT_DESC_OWN));

            in->buffer = (void *)pending.data;
            in->desc = bdt_descriptor(n, data1);
            in->desc |= BDT_DESC_OWN;

            pending.data += n;
            pending.length -= n;

            toggle_oddbit(CONTROL_ENDPOINT, 0);
        } else {
            struct bdtentry *out =
                bdt_entry(CONTROL_ENDPOINT, 1, oddbit(CONTROL_ENDPOINT, 1));

            /* Prepare to send a zero-length DATA1 packet to
             * aknowledge proper reception during the preceding
             * OUT phase. */

            uassert(!(out->desc & BDT_DESC_OWN));

            out->buffer = NULL;
            out->desc = bdt_descriptor(0, 1);
            out->desc |= BDT_DESC_OWN;

            toggle_oddbit(CONTROL_ENDPOINT, 1);
            phase = PHASE_STATUS_OUT;
        }
    }

    if (pid == BDT_PID_SETUP) {
        USB0_CTL &= ~USB_CTL_TXSUSPENDTOKENBUSY;
    }

    if ((phase == PHASE_STATUS_IN && pid == BDT_PID_OUT) ||
        (phase == PHASE_STATUS_OUT && pid == BDT_PID_IN)) {

        /* Set the address. */

        if (USB0_ADDR != address) {
            uassert(phase == PHASE_STATUS_OUT && pid == BDT_PID_IN);
            USB0_ADDR = address;
        }

        phase = PHASE_COMPLETE;
    }

    if (phase == PHASE_COMPLETE) {
        struct bdtentry *in =
            bdt_entry(CONTROL_ENDPOINT, 0, oddbit(CONTROL_ENDPOINT, 0));

        /* Prepare an input buffer for the next setup token. */

        uassert(!(in->desc & BDT_DESC_OWN));

        in->buffer = (void *)buffers.setup;
        in->desc = bdt_descriptor(8, 0);
        in->desc |= BDT_DESC_OWN;

        toggle_oddbit(CONTROL_ENDPOINT, 0);

        phase = PHASE_SETUP;
    }
}

__attribute__((interrupt ("IRQ"))) void usb_isr(void)
{
    if (USB0_ISTAT & USB_ISTAT_USBRST) {
        /* Reset the even/odd BDT toggle bits. */

        USB0_CTL |= USB_CTL_ODDRST;

        phase = PHASE_SETUP;
        address = 0;

        pending.data = NULL;
        pending.length = 0;

        reset_oddbit(CONTROL_ENDPOINT, 0);
        reset_oddbit(CONTROL_ENDPOINT, 1);

        /* Initialize the BDT entries for endpoint 0. */

        for (int i = 0 ; i < 2 ; i += 1) {
            struct bdtentry *r = bdt_entry(CONTROL_ENDPOINT, 0, i);
            struct bdtentry *t = bdt_entry(CONTROL_ENDPOINT, 1, i);

            if (i == 0) {
                /* Prepare a buffer for the first setup packet (setup
                 * packets are always DATA0). */

                r->buffer = buffers.setup;
                r->desc = bdt_descriptor(8, 0);
                r->desc |= BDT_DESC_OWN;

                toggle_oddbit(CONTROL_ENDPOINT, 0);
            } else {
                r->buffer = NULL;
                r->desc = bdt_descriptor(0, 0);
            }

            t->buffer = NULL;
            t->desc = bdt_descriptor(0, 0);
        }

        /* Activate endpoint 0, reset all error flags and reset
         * address to 0. */

        USB0_ENDPT(CONTROL_ENDPOINT) = (USB_ENDPT_EPRXEN | USB_ENDPT_EPTXEN |
                                        USB_ENDPT_EPHSHK);
        USB0_ERRSTAT = ~0;
        USB0_ADDR = 0;

        /* Enable interesting interrupts. */

        USB0_ERREN = ~0;
        USB0_INTEN = (USB_INTEN_USBRSTEN |
                      USB_INTEN_ERROREN |
                      USB_INTEN_TOKDNEEN |
                      USB_INTEN_SLEEPEN |
                      USB_INTEN_STALLEN);

        USB0_CTL &= ~(USB_CTL_ODDRST | USB_CTL_TXSUSPENDTOKENBUSY);

        USB0_ISTAT |= USB_ISTAT_USBRST;
    } else if (USB0_ISTAT & USB_ISTAT_ERROR) {
        USB0_ERRSTAT = ~0;
        USB0_ISTAT |= USB_ISTAT_ERROR;
    } else if (USB0_ISTAT & USB_ISTAT_STALL) {
        for (int i = 0 ; i < ENDPOINTS_N ; i += 1) {
            USB0_ENDPT(i) &= ~USB_ENDPT_EPSTALL;
        }

        USB0_ISTAT |= USB_ISTAT_STALL;
    } else if (USB0_ISTAT & USB_ISTAT_SLEEP) {
        USB0_ISTAT |= USB_ISTAT_SLEEP;
    } else while (USB0_ISTAT & USB_ISTAT_TOKDNE) {
        const uint8_t status = USB0_STAT;
        const int n = USB_STAT_ENDP(status);

        /* A new token has been received. */

        switch (n) {
        case CONTROL_ENDPOINT: handle_control_transfer(status); break;
        case DATA_ENDPOINT: handle_data_transfer(status); break;
        case NOTIFICATION_ENDPOINT: break;
        default:
            USB0_ENDPT(n) |= USB_ENDPT_EPSTALL;
        }

        USB0_ISTAT |= USB_ISTAT_TOKDNE;
    }
}

void reset_usb()
{
    /* Enable the USB interrupt and allow it to preempt all but the
     * critical stuff. */

    prioritize_interrupt(USB0_IRQ, 2);
    enable_interrupt(USB0_IRQ);

    /* Set the USB clock source to MGCPLL with a divider for 48Mhz and
     * enable the USB clock. */

#ifdef TEENSY30
    SIM_CLKDIV2 |= SIM_CLKDIV2_USBDIV(0);
#else
    SIM_CLKDIV2 |= SIM_CLKDIV2_USBDIV(2);
#endif
    SIM_SOPT2 |= SIM_SOPT2_USBSRC | SIM_SOPT2_PLLFLLSEL;
    SIM_SCGC4 |= SIM_SCGC4_USBOTG;

    /* Reset the USB module.  The datasheet specifies 2 USB clock
     * periods (which equal 3 system clock periods at 72Mhz) of
     * waiting time after setting this bit. */

    USB0_USBTRC0 |= USB_USBTRC0_USBRESET;
    __asm__ volatile ("nop");
    __asm__ volatile ("nop");
    __asm__ volatile ("nop");

    /* Set the BDT base address which is aligned on a 512-byte
     * boundary and hence its first 9 bits are identically zero. */

    USB0_BDTPAGE1 = (uint8_t)((((uint32_t)bdt) >> 8) & 0xfe);
    USB0_BDTPAGE2 = (uint8_t)(((uint32_t)bdt) >> 16);
    USB0_BDTPAGE3 = (uint8_t)(((uint32_t)bdt) >> 24);

    /* Clear all USB ISR flags and enable weak pull-downs. */

    USB0_ISTAT = ~0;
    USB0_ERRSTAT = ~0;
    USB0_OTGISTAT = ~0;
    USB0_USBCTRL = 0;

    /* Enable the USB module. */

    USB0_CTL = USB_CTL_USBENSOFEN;

    /* Enable USB reset interrupt. */

    USB0_INTEN = USB_INTEN_USBRSTEN;

    /* Enable d+ pull-up. */

    USB0_CONTROL |= USB_CONTROL_DPPULLUPNONOTG;
}

int is_usb_enumerated()
{
    return configuration;
}

void await_usb_enumeration()
{
    while (configuration == 0);
}

int interrupt_usb(uint8_t *buffer, size_t n)
{
    volatile struct bdtentry *out =
        bdt_entry(NOTIFICATION_ENDPOINT, 1, oddbit(NOTIFICATION_ENDPOINT, 1));

    while (out->desc & BDT_DESC_OWN);

    memcpy(out->buffer, buffer, n);

    out->desc = bdt_descriptor(n, oddbit(NOTIFICATION_ENDPOINT, 1));
    out->desc |= BDT_DESC_OWN;

    toggle_oddbit(NOTIFICATION_ENDPOINT, 1);

    return 0;
}

int write_usb(const char *s, int n, bool flush)
{
    int m;

    if (configuration == 0) {
        return -1;
    }

    /* If the current buffer has been filled and flushed to the USB
     * module fetch a new one. */

    if (transmit.length == DATA_BUFFER_SIZE) {
        volatile struct bdtentry *out =
            bdt_entry(DATA_ENDPOINT, 1, oddbit(DATA_ENDPOINT, 1));

        transmit.data = out->buffer;
        transmit.length = 0;

        while (out->desc & BDT_DESC_OWN);
        uassert(transmit.length != DATA_BUFFER_SIZE);
    }

    /* Break the write up if it's too large to fit in the current
     * buffer. */

    if (n > 0) {
        m = DATA_BUFFER_SIZE - transmit.length < n ?
            DATA_BUFFER_SIZE - transmit.length : n;
    } else {
        m = 0;
    }

    uassert(m > 0 || n == 0);

    if (m > 0) {
        memcpy(transmit.data, s, m);

        transmit.data += m;
        transmit.length += m;
    }

    /* Send the current buffer to the USB module. */

    if (flush || transmit.length == DATA_BUFFER_SIZE) {
        volatile struct bdtentry *out =
            bdt_entry(DATA_ENDPOINT, 1, oddbit(DATA_ENDPOINT, 1));

        uassert(!(out->desc & BDT_DESC_OWN));

        out->desc = bdt_descriptor(transmit.length,
                                    oddbit(DATA_ENDPOINT, 1));
        out->desc |= BDT_DESC_OWN;

        toggle_oddbit(DATA_ENDPOINT, 1);
    }

    if (flush) {
        transmit.length = DATA_BUFFER_SIZE;
    }

    if (n > m) {
        return write_usb(s + m, n - m, flush);
    } else {
        return 0;
    }
}

int set_usb_serial_state(uint16_t state)
{
    uint16_t notification[5];

    notification[0] = NOTIFICATION_SERIALSTATE;
    notification[1] = 0;
    notification[2] = COMMUNICATION_INTERFACE;
    notification[3] = 2;
    notification[4] = state;

    interrupt_usb((uint8_t *)notification, 10);

    return 0;
}

void set_usb_data_in_callback(void (*new_callback)(uint8_t *data, size_t n))
{
    data_in_callback = new_callback;
}

void await_usb_dtr()
{
    while (!(line_state & LINE_STATE_DTR));
}

int is_usb_dtr()
{
    return line_state & LINE_STATE_DTR;
}

void await_usb_rts()
{
    while (!(line_state & LINE_STATE_RTS));
}

int is_usb_rts()
{
    return line_state & LINE_STATE_RTS;
}

static void utostr(uint64_t n, unsigned int radix, int width, int precision)
{
    if (n >= radix || width > 1 || precision > 1) {
        utostr(n / radix, radix, width - 1, precision - 1);
    }

    if (n == 0) {
        write_usb(precision > 0 ? "0" : " ", 1, false);
    } else {
        n %= radix;
        const char c = n + (n < 10 ? '0' : 'a' - 10);
        write_usb(&c, 1, false);
    }
}

static void itostr(int64_t n, unsigned int radix, int width, int precision)
{
    if (n < 0) {
        write_usb("-", 1, false);
        utostr((uint64_t)(-n), radix, width, precision);
    } else {
        utostr((uint64_t)n, radix, width, precision);
    }
}

static void ftostr(double n, int width, int precision)
{
    if (isnan(n)) {
        write_usb("nan", 3, true);
        return;
    }

    {
        const int i = isinf(n);

        if (i) {
            write_usb(i > 0 ? "+" : "-", 1, false);
            write_usb("inf", 3, true);
            return;
        }
    }

    if (n < 0) {
        write_usb("-", 1, false);

        utostr(-n, 10, width - precision - 1 - (n < 0), 1);
    } else {
        utostr(n, 10, width - precision - 1 - (n < 0), 1);
    }

    const double f = fmod(n, 1);

    if (f == 0) {
        return;
    }

    write_usb(".", 1, false);

    {
        int i;
        uint32_t u;

        for (u = fabs(f * pow(10, precision)), i = 0;
             u > 0 && u % 10 == 0;
             u /= 10, i++);

        if (!u) {
            write_usb("0", 1, true);
        } else {
            utostr(u, 10, 0, precision - i);
        }
    }
}

int uprintf(const char *format, ...)
{
    va_list ap;
    const char *c, *d;

    if (!is_usb_dtr()) {
        return -1;
    }

    va_start(ap, format);

    for (c = format ; *c != '\0' ;) {
        for (d = c; *d != '%' && *d != '\0' ; d += 1);

        if (d > c) {
            write_usb(c, d - c, false);
        }

        c = d;

        if (*c == '%') {
            int size = 2, base = -1, sign = -1, width = -1, precision = -1;

            for (c += 1 ; ; c += 1) {
                if (*c == '%') {
                    write_usb("%", 1, false);
                    break;
                } else if (((*c >= '0' && *c <= '9') || *c == '.') &&
                           width < 0) {
                    if (*c != '.') {
                        width = strtol(c, (char **)&c, 10);
                    }

                    if (*c == '.') {
                        precision = strtol(c + 1, (char **)&c, 10);
                    }

                    c -= 1;
                } else if (*c == 'l') {
                    size += 1;
                } else if (*c == 'h') {
                    size -= 1;
                } else {
                    if (*c == 's') {
                        char *s;

                        s = va_arg(ap, char *);
                        write_usb(s, width > 0 ? width : (int)strlen(s), false);
                        break;
                    } else if (*c == 'f') {
                        ftostr(
                            va_arg(ap, double),
                            width, precision > 0 ? precision : 6);
                        break;
                    } else if (*c == 'd') {
                        base = 10;
                        sign = 1;
                    } else if (*c == 'u') {
                        base = 10;
                        sign = 0;
                    } else if (*c == 'x') {
                        base = 16;
                        sign = 0;
                    } else if (*c == 'b') {
                        base = 2;
                        sign = 0;
                    } else {
                        break;
                    }

                    if (precision < 0) {
                        precision = 1;
                    }

                    if (sign) {
                        switch (size) {
                        case 3:
                            itostr(va_arg(ap, int64_t), base, width, precision);
                            break;
                        case 2:
                            itostr(va_arg(ap, int32_t), base, width, precision);
                            break;
                        default:
                            itostr(va_arg(ap, int), base, width, precision);
                            break;
                        }
                    } else {
                        switch (size) {
                        case 3:
                            utostr(va_arg(ap, uint64_t), base, width, precision);
                            break;
                        case 2:
                            utostr(va_arg(ap, uint32_t), base, width, precision);
                            break;
                        default:
                            utostr(va_arg(ap, unsigned int), base, width, precision);
                            break;
                        }
                    }

                    break;
                }
            }

            c += 1;
        }
    }

    va_end(ap);

    write_usb(NULL, 0, true);

    return 0;
}

void __attribute__((noreturn)) _uassert(
    const char *msg, int line, const char *func)
{
    set_led();

    disable_all_interrupts();
    prioritize_interrupt(USB0_IRQ, 0);
    enable_interrupt(USB0_IRQ);

    await_usb_dtr();

    uprintf(msg, line, func);

    disable_interrupt(USB0_IRQ);

    __asm__ volatile ("bkpt #251");

    while(1);
}
