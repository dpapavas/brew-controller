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

#include "mk20dx.h"
#include "callbacks.h"
#include "filter.h"
#include "time.h"
#include "usb.h"

struct filter temperature_filter = DOUBLE_FILTER(1.0, 60.0);

#define TRANSMIT(X, FLAGS) {                                            \
        SPI0_SR |= SPI_SR_TFFF;                                         \
        WAIT_WHILE(!(SPI0_SR & SPI_SR_TFFF), 100);                      \
        SPI0_PUSHR = SPI_PUSHR_PCS(0) | SPI_PUSHR_CTAS(1) | (FLAGS) | (X); \
    }

void run_temperature(bool run)
{
    /* Configure for automatic conversion, 3-wire RTD, 50Hz filter
     * notch. */

    TRANSMIT(0x80, SPI_PUSHR_CONT);
    TRANSMIT(run ? 0b11010001 : 0b00010011, 0);

  error:
}

void read_temperature()
{
    TRANSMIT(0x80, SPI_PUSHR_CONT);
    TRANSMIT(0b10110001, 0);

  error:
}

void reset_temperature(void)
{
    SPI0_MCR |= SPI_MCR_PCSIS(0);

    /* Baud rate: (48Mhz / 3) * (1 / 8) = 48 / 24 = 2Mhz */
    /* PCS to SCK Delay (tCSC): 1 / 48Mhz * 3 * 8 = 500ns (specs say 400ns) */
    /* After SCK Delay (tASC): 1 / 48Mhz * 3 * 2 = 125ns (specs say 100ns) */
    /* Delay after Transfer (tDT): 1 / 48Mhz * 3 * 8 = 500ns (specs say 400ns) */

    SPI0_CTAR1 = (SPI_CTAR_FMSZ(7) | SPI_CTAR_PBR(1) | SPI_CTAR_BR(3)
                  | SPI_CTAR_PCSSCK(1) | SPI_CTAR_CSSCK(2)
                  | SPI_CTAR_PASC(1) | SPI_CTAR_ASC(0)
                  | SPI_CTAR_PDT(1) | SPI_CTAR_DT(2)
                  | SPI_CTAR_CPHA);

    SIM_SCGC5 |= SIM_SCGC5_PORTC;
    PORTC_PCR3 = PORT_PCR_MUX(1) | PORT_PCR_IRQC(8) | PORT_PCR_PFE; /* DRDY */
    PORTC_PCR4 = PORT_PCR_MUX(2) | PORT_PCR_DSE; /* CS0 */

    delay_ms(100);

    run_temperature(true);

    /* Read the high data byte to reset the DRDY signal.  (This is
     * only necessary for convenience, as when repogramming the MCU,
     * the MAX13865 remains powered and active, so the DRDY may be
     * already low when the MCU boots up so no interrupt will be
     * generated.) */

    WAIT_WHILE(GPIOC_PDIR & PT(3), 50000);

    TRANSMIT(0x01, SPI_PUSHR_CONT);
    TRANSMIT(0, 0);

  error:
    prioritize_interrupt(PORTC_IRQ, 8);
    enable_interrupt(PORTC_IRQ);
}

__attribute__((interrupt ("IRQ"))) void portc_isr(void)
{
    /* At this point, the current state of the SPI module is not
     * known; it might be busy sending data elsewhere, with one or
     * more items queued in its TX FIFO. Send the data register's read
     * address, setting the EOQ flag, so that we can wait for its
     * completion. */

    TRANSMIT(0x01, SPI_PUSHR_CONT | SPI_PUSHR_EOQ);

    /* Wait for the transmission to end and flush the RX FIFO.  */

    WAIT_WHILE(!(SPI0_SR & SPI_SR_EOQF), 100);
    SPI0_MCR |= SPI_MCR_CLR_RXF;
    SPI0_SR |= SPI_SR_EOQF;

    /* Send two dummy bytes, to get the two bytes of temperature
     * data. */

    TRANSMIT(0, SPI_PUSHR_CONT);
    TRANSMIT(0, SPI_PUSHR_EOQ);

    WAIT_WHILE(!(SPI0_SR & SPI_SR_EOQF), 100);

    const uint8_t a = SPI0_POPR;
    const uint8_t b = SPI0_POPR;

    SPI0_SR |= SPI_SR_EOQF;
    PORTC_PCR3 |= PORT_PCR_ISF;

    /* Convert and filter the sample. */

    const uint16_t c = (a << 8) | b;
    double T;

    if (c & 1) {
        T = NAN;

        if (is_usb_dtr()) {
            /* Send the fault register's read address and read the next two
             * bytes of returned data. */

            SPI0_PUSHR = SPI_PUSHR_PCS(0) | SPI_PUSHR_CTAS(1) | SPI_PUSHR_CONT | 0x07;
            SPI0_PUSHR = SPI_PUSHR_PCS(0) | SPI_PUSHR_CTAS(1) | 0;

            WAIT_WHILE(!(SPI0_SR & SPI_SR_RFDF), 100);
            SPI0_POPR;

            SPI0_SR |= SPI_SR_RFDF;
            WAIT_WHILE(!(SPI0_SR & SPI_SR_RFDF), 100);

            uprintf("MAX13865 fault: %b\n", SPI0_POPR);
        }

        run_temperature(false);
        delay_ms(100);
        run_temperature(true);
    } else {
        const double A = 3.9083e-3;
        const double B = -5.775e-7;

        T = (
            (-A + sqrt(A * A - 4 * B * (1 - ldexp(c >> 1, -15) * 4.3)))
            / (2 * B));
    }

    /* Occasionally, one of the two bytes making up the 16-bit code is
     * read as zero.  It is not clear why that happens and, since it
     * happens relatively rarely (i.e. once in several tens of
     * thousands of conversions, we'll just discard these readings in
     * software. */

    if ((a && b) || (fabs(T - temperature_filter.y) < 1)) {
        filter_sample(&temperature_filter, T, get_time());
    }

    RUN_CALLBACKS(
        temperature_callbacks,
        bool (*)(double, double, double, double, double, int32_t),
        temperature_filter.y, temperature_filter.dy,
        temperature_filter.t, temperature_filter.dt,
        T, c);

  error:
}

double get_temperature(void)
{
    return temperature_filter.y;
}

#undef WAIT_WHILE
#undef TRANSMIT
