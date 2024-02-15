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

#include <stdbool.h>
#include <math.h>

#include "callbacks.h"
#include "mk20dx.h"
#include "usb.h"

static double heat = 1, pump = 1;

__attribute__((interrupt ("IRQ"))) void porta_isr(void)
{
    PORTA_PCR12 |= PORT_PCR_ISF;

    const bool p = is_interrupt_enabled(PIT0_IRQ);
    const bool q = is_interrupt_enabled(PIT1_IRQ);

    /* If both timer interrupts are disabled, disable the
     * zero-crossing interrupt as well. */

    if (!(p || q)) {
        disable_interrupt(PORTA_IRQ);
        return;
    }

    /* Set the trigger pin to input to stop the current pulse and arm
     * the timer for the next one. */

    /* Heat */

    if (p) {
        GPIOB_PCOR = PT(17);
        PIT_TCTRL(0) |= PIT_TCTRL_TEN;
    }

    /* Pump */

    if (q) {
        GPIOD_PCOR = PT(0);
        PIT_TCTRL(1) |= PIT_TCTRL_TEN;
    }
}

static double calculate_delay(double P)
{
    /* The power for a given firing angle theta is given by:
     *
     *   P = 1 + (sin(2 theta) - 2 theta) / (2 pi)
     *
     * Bisect it to find the firing angle (c below is 2 theta)
     * for the requested power and convert it to a delay. */

    const double epsilon = 1e-3;
    double a = 0, b = 2 * M_PI;

    while (true) {
        const double c = (a + b) / 2;
        const double f = 1 + (sin(c) - c) / (2 * M_PI) - P;

        if (fabs(f) < epsilon) {
            return c / 2 / M_PI;
        }

        if (f > 0) {
            a = c;
        } else {
            b = c;
        }
    }
}

/* Very short trigger pulses can lead to spurious firing.  This
 * seems to happen in two ways:
 *
 * 1. When the pulse is smaller than 1% of the half-cycle, the PIT
 * interrupt may arrive after the Port C interrupt, enabling the
 * output for the whole next half-cycle.
 *
 * 2. When the pulse is smaller than ~2%, depending on load
 * current, the TRIAC fires spuriously.  This seems to be related
 * to the critical dI/dt value of the TRIAC.
 *
 * At any rate, 5% duty corresponds to less than 1mW RMS, so we
 * simply turn off the TRIAC below that. */

#define DEFINE_FUNCTIONS(WHAT, GPIO, PIN, PIT, COND)                    \
    __attribute__((interrupt ("IRQ"))) void pit## PIT ##_isr(void)      \
    {                                                                   \
        PIT_TFLG(PIT) |= PIT_TFLG_TIF;                                  \
        PIT_TCTRL(PIT) &= ~PIT_TCTRL_TEN;                               \
                                                                        \
        if (COND) {                                                     \
            GPIO ##_PSOR = PT(PIN);                                     \
        }                                                               \
    }                                                                   \
                                                                        \
    void set_## WHAT ##_delay(double d)                                 \
    {                                                                   \
        WHAT = d;                                                       \
                                                                        \
        if (isnan(d) || d == 0 || d >= 0.95) {                          \
            disable_interrupt(PIT0_IRQ + PIT);                          \
                                                                        \
            if (d == 0 && COND) {                                       \
                GPIO ##_PSOR = PT(PIN);                                 \
            } else {                                                    \
                GPIO ##_PCOR = PT(PIN);                                 \
            }                                                           \
                                                                        \
            return;                                                     \
        }                                                               \
                                                                        \
        PIT_LDVAL(PIT) = (unsigned int)(d * 480000) - 1;                \
        PIT_TFLG(PIT) |= PIT_TFLG_TIF;                                  \
                                                                        \
        enable_interrupt(PIT0_IRQ + PIT);                               \
        enable_interrupt(PORTA_IRQ);                                    \
    }                                                                   \
                                                                        \
    void set_## WHAT ##_power(double P)                                 \
    {                                                                   \
        set_## WHAT ##_delay(                                           \
            P <= 0 || P >= 1 ? (P <= 0) : calculate_delay(P));          \
    }                                                                   \
                                                                        \
    double get_## WHAT ##_duty()                                        \
    {                                                                   \
        return 1 - WHAT;                                                \
    }                                                                   \
                                                                        \
    double get_## WHAT ##_power()                                       \
    {                                                                   \
        const double phi = 2 * WHAT * M_PI;                             \
        return 1 + (sin(phi) - phi) / (2 * M_PI);                       \
    }

DEFINE_FUNCTIONS(heat, GPIOB, 17, 0, true)
DEFINE_FUNCTIONS(pump, GPIOD, 0, 1, !(GPIOB_PDIR & PT(16)))

#undef DEFINE_FUNCTIONS

void set_pump_flow(double Q)
{
    /* There's virtually no flow below a power of 25%.  After that the
     * power-to-flow relationship is remarkably linear.  This simple
     * transformation provides a "flow" setting. */

    if (!Q) {
        set_pump_delay(1);
    } else {
        set_pump_power(0.75 * Q + 0.25);
    }
}

double get_pump_flow(void)
{
    return fmax((get_pump_power() - 0.25) / 0.75, 0);
}

static bool panel_callback(bool down)
{
    set_pump_delay(pump);

    return false;
}

void reset_power(void)
{
    SIM_SCGC5 |= SIM_SCGC5_PORTA | SIM_SCGC5_PORTB | SIM_SCGC5_PORTD;

    /* Configure trigger pins as GPIO, defaulting to pull-down inputs,
     * but set to go high once switched to output. */

    PORTB_PCR17 |= PORT_PCR_MUX(1) | PORT_PCR_PE;
    GPIOB_PDDR |= PT(17);
    GPIOB_PCOR = PT(17);

    PORTD_PCR0 |= PORT_PCR_MUX(1) | PORT_PCR_PE;
    GPIOD_PDDR |= PT(0);
    GPIOD_PCOR = PT(0);

    /* Condfigure zero-crossing pin as input with rising edge interrupt. */

    PORTA_PCR12 |= PORT_PCR_MUX(1) | PORT_PCR_IRQC(9) | PORT_PCR_PFE;

    /* Confgiure port A interrupt with highest priority, since we need
     * the zero-crossing interrupt to be able to preempt all
     * others. */

    prioritize_interrupt(PORTA_IRQ, 0);

    /* Configure PIT channels 0, 1 interrupt with preempting
     * priority. */

    PIT_TCTRL(0) = PIT_TCTRL_TIE;
    PIT_TCTRL(1) = PIT_TCTRL_TIE;

    prioritize_interrupt(PIT0_IRQ, 1);
    prioritize_interrupt(PIT1_IRQ, 1);

    add_callback(panel_callback, panel_callbacks);
}
