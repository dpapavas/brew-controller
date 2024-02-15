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
#include <string.h>

#include "callbacks.h"
#include "filter.h"
#include "mk20dx.h"
#include "time.h"
#include "uassert.h"

struct filter flow_filter = SINGLE_FILTER(0.15);

static int32_t pulses;
static double flow = NAN, derivative = NAN, volume = NAN;

__attribute__((interrupt ("IRQ"))) void ftm0_isr(void)
{
    static uint16_t overflows[2];
    const double t = get_time();

    /* Count the overflows before and after the first edge.  We
     * consider more than 1 overflow at either phase as signifying
     * stagnation.  (Within the period, this may be the case even with
     * only 1; see below).

     * If both overflow and capture interrupt flags were raised on
     * entry, there's a potential race.  Maybe the overflow came
     * before the capture and should therefore be reset, but it might
     * have come after it, while the ISR, was waiting to execute.  In
     * the latter case the overflow must be considered in the period
     * calculation.

     * Since an overflow less can lead to a wrong period calculation,
     * while an overflow more can only lead to a skipped sample, we
     * always assume the overflow came after the capture. */

    if (FTM0_SC & FTM_SC_TOF) {
        FTM0_SC &= ~FTM_SC_TOF;
        overflows[(FTM0_C0SC & FTM_CSC_CHF) > 0] += 1;
    }

    bool stagnated = (overflows[1] > 1);

    if (FTM0_C1SC & FTM_CSC_CHF) {
        FTM0_C0SC &= ~FTM_CSC_CHF;
        FTM0_C1SC &= ~FTM_CSC_CHF;

        const uint16_t t_0 = FTM0_C0V;
        const uint16_t t_1 = FTM0_C1V;

        /* The 16-bit unsigned arithmetic below calculates the correct
         * signal period, as long as it is below the maximum timer
         * period, i.e. 65536/375000, or about 17ms.  This is the case
         * when there are 0 overflows, or when there's 1, but the end
         * time, hasn't caught up with the start time yet.

         * We consider anything else "stagnation", i.e. no flow, or
         * too low to bother about measuring. */

        stagnated = (stagnated || (overflows[1] == 1 && t_1 >= t_0));

        overflows[0] = overflows[1] = 0;
        pulses++;

        if (!stagnated) {
            const double r = 375e3 / (uint16_t)(t_1 - t_0);

            filter_sample(&flow_filter, r, t);

            const double y = flow_filter.y;
            const double dy = flow_filter.dy;
            const double dt = flow_filter.dt;

#define A 1.8021e-06
#define B -3.7826e-04
#define C 1.1075e-01

            const double dV = A * y * y + B * y + C;
            const double ddV = A * dy * (2 * y + dy) + B * dy;
#undef A
#undef B
#undef C

            flow = dV / dt;
            const double delta = ddV / dt;

            volume += dV;
            derivative = delta / dt;

            RUN_CALLBACKS(
                flow_callbacks,
                bool (*)(double, double, double, double, double, int32_t),
                flow, delta, flow_filter.t, dt, r, pulses);
        }
    }

    if (stagnated || overflows[0] > 1) {
        filter_sample(&flow_filter, 0, t);

        flow = derivative = NAN;

        RUN_CALLBACKS(
            flow_callbacks,
            bool (*)(double, double, double, double, double, int32_t),
            0, 0, flow_filter.t, flow_filter.dt, 0, pulses);
    }
}

void reset_flow(void)
{
    SIM_SCGC5 |= SIM_SCGC5_PORTC | SIM_SCGC5_PORTD;
    SIM_SCGC6 |= SIM_SCGC6_FTM0;

    /* Sensor power */

    PORTD_PCR5 = PORT_PCR_MUX(1) | PORT_PCR_DSE;
    PORTD_PCR6 = PORT_PCR_MUX(1) | PORT_PCR_DSE;
    GPIOD_PDDR |= (PT(5) | PT(6));
    GPIOD_PSOR = PT(5);
    GPIOD_PCOR = PT(6);

    /* Sensor signal */

    PORTC_PCR1 |= PORT_PCR_MUX(4);

    /* Configure the timer for input capture.  Set system clock source with
     * 1/128 prescaler for a clock frequency of 48 Mhz / 128 =
     * 375Khz */

    FTM0_MODE |= FTM_MODE_WPDIS;
    FTM0_MODE |= FTM_MODE_FTMEN;

    FTM0_COMBINE |= (FTM_COMBINE_DECAPEN0 | FTM_COMBINE_DECAP0);
    FTM0_FILTER = FTM_FILTER_CH0FVAL(15) | FTM_FILTER_CH1FVAL(15);
    FTM0_MOD = 65535;
    FTM0_CNT = 0;

    FTM0_C0SC = FTM_CSC_MSA | FTM_CSC_ELSB;
    FTM0_C1SC = FTM_CSC_ELSB | FTM_CSC_CHIE;
    FTM0_SC = FTM_SC_CLKS(1) | FTM_SC_PS(7) | FTM_SC_TOIE;

    prioritize_interrupt(FTM0_IRQ, 8);
    enable_interrupt(FTM0_IRQ);
}

void tare_flow(void)
{
    pulses = 0;
    volume = 0;
}

double get_flow(void)
{
    return flow;
}

double get_flow_derivative(void)
{
    return derivative;
}

double get_volume(void)
{
    return volume;
}
