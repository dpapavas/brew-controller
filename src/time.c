#include <stdbool.h>

#include "mk20dx.h"
#include "callbacks.h"
#include "time.h"

static volatile uint32_t ticks;

__attribute__((interrupt ("IRQ"))) void systick_isr(void)
{
    ticks++;
}

__attribute__((interrupt ("IRQ"))) void ftm1_isr(void)
{
    FTM1_SC &= ~FTM_SC_TOF;

    RUN_CALLBACKS(tick_callbacks, bool (*)());
}

double get_time()
{
    const uint32_t r = SYST_RVR;
    uint32_t t, u;

    do {
        t = ticks;
        u = SYST_CVR;
    } while (t != ticks);

    return t / 10.0 + (double)(r - u) / COUNTS_PER_US / 1e6;
}

void delay_us(int32_t n)
{
    const uint32_t t = ticks;
    const int32_t r = SYST_RVR, u = SYST_CVR;

    /* There is a potential race here: `ticks` may be read first,
     * followed by an interrupt that increases it, followed by a read
     * of `SYST_CVR` which has now wrapped around.  This leads to a
     * wrong calculation, albeit one that is always lower than the
     * correct one and therefore cannot break the loop.  Care must be
     * taken though to use signed arithmetic, as otherwise `u -
     * SYST_CVR` would be interpreted as a large positive number,
     * although it's really negative. */

    while (((int32_t)(ticks - t) * (r + 1) + u - (int32_t)SYST_CVR)
           / COUNTS_PER_US < n);
}

void reset_time()
{
    /* Configure FTM1 as a 10Hz tick interrupt, for profile
     * execution and such. */

    SIM_SCGC6 |= SIM_SCGC6_FTM1;
    FTM1_SC = FTM_SC_CLKS(1) | FTM_SC_PS(7) | FTM_SC_TOIE;
    FTM1_MOD = 37499;

    prioritize_interrupt(FTM1_IRQ, 8);
    enable_interrupt(FTM1_IRQ);
}
