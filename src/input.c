#include <stdbool.h>

#include "callbacks.h"
#include "mk20dx.h"
#include "usb.h"
#include "time.h"
#include "uassert.h"

static int delta, panel_button, encoder_button;

void reset_input(void)
{
    SIM_SCGC5 |= SIM_SCGC5_PORTD | SIM_SCGC5_PORTB;

    /* We detect rising or falling edges on buttons (instead of, more
     * conveniently, both) to reject noise glitches.  Pull-ups on the
     * encoder are weak enough for noise from nearby motors etc. to be
     * able to pull the switch inputs low.  This would normally be
     * taken care of by debouncing, but the noise can be fast enough,
     * so that by the time the falling edge interrupt executes, the
     * pin level is high again.  This is then mistaken for a rising
     * edge, which is confirmed after the debouncing interval, when
     * the noise transient has ended and the input has been pulled
     * high again. */

    /* Panel button */

    PORTB_PCR16 = (PORT_PCR_MUX(1) | PORT_PCR_IRQC(10) | PORT_PCR_PFE
                   | PORT_PCR_SRE | PORT_PCR_PE | PORT_PCR_PS);

    /* Encoder button */

    PORTD_PCR4 = (PORT_PCR_MUX(1) | PORT_PCR_IRQC(10) | PORT_PCR_PFE
                  | PORT_PCR_SRE);

    /* If the button is pressed during boot, wait for USB attachment
     * and enter programming mode. */

    delay_ms(10);

    if (!(GPIOD_PDIR & PT(4))) {
        await_usb_enumeration();
         __asm__ volatile ("bkpt #251");
    }

    /* Encoder pulses */

    PORTD_PCR2 = (PORT_PCR_MUX(1) | PORT_PCR_IRQC(11) | PORT_PCR_PFE
                  | PORT_PCR_SRE);
    PORTD_PCR3 = (PORT_PCR_MUX(1) | PORT_PCR_IRQC(11) | PORT_PCR_PFE
                  | PORT_PCR_SRE);

    /* PIT timers are clocked by the bus clock.  For an interval of x
     * seconds the needed LDVAL is x * 48000000. */

    /* Encoder rotation debounce timer (5ms) */

    PIT_LDVAL(2) = 240000 - 1;
    PIT_TCTRL(2) = PIT_TCTRL_TIE;

    /* Button debounce timer (5ms) */

    PIT_LDVAL(3) = 240000 - 1;
    PIT_TCTRL(3) = PIT_TCTRL_TIE;

    /* Port B interrupt */

    prioritize_interrupt(PORTB_IRQ, 10);
    enable_interrupt(PORTB_IRQ);

    /* Port D interrupt */

    prioritize_interrupt(PORTD_IRQ, 10);
    enable_interrupt(PORTD_IRQ);

    /* PIT channels 2 and 3 interrupts */

    prioritize_interrupt(PIT2_IRQ, 10);
    enable_interrupt(PIT2_IRQ);

    prioritize_interrupt(PIT3_IRQ, 10);
    enable_interrupt(PIT3_IRQ);
}

__attribute__((interrupt ("IRQ"))) void pit2_isr(void)
{
    /* If end-of-rotation-cycle debouncing is complete, reset the
     * current rotation direction and disable the timer. */

    PIT_TFLG(2) |= PIT_TFLG_TIF;
    PIT_TCTRL(2) &= ~PIT_TCTRL_TEN;

    RUN_CALLBACKS(turn_callbacks, bool (*)(int), delta);

    delta = 0;
}

__attribute__((interrupt ("IRQ"))) void pit3_isr(void)
{
    /* Register the click and disable the timer. */

    PIT_TFLG(3) |= PIT_TFLG_TIF;
    PIT_TCTRL(3) &= ~PIT_TCTRL_TEN;

    /* PCR & IRQC(1) tests the first bit of the IRQC field, to
     * determine whether we've detected a rising or falling edge, then
     * makes sure the current pin level matches that (low level for
     * falling edge and vice versa).
     *
     * PCR ^ IRQC(3) flips the first two bits and hence the detected
     * edge.*/

    if (panel_button && (
            ((PORTB_PCR16 & PORT_PCR_IRQC(1)) == 0)
            == (((GPIOB_PDIR & PT(16)) == 0)))) {
        RUN_CALLBACKS(
            panel_callbacks, bool (*)(bool),
            (PORTB_PCR16 & PORT_PCR_IRQC(1)) == 0);

        PORTB_PCR16 ^= PORT_PCR_IRQC(3);
    }

    if (encoder_button && (
            ((PORTD_PCR4 & PORT_PCR_IRQC(1)) == 0)
            == (((GPIOD_PDIR & PT(4)) == 0)))) {
        RUN_CALLBACKS(
            click_callbacks, bool (*)(bool),
            (PORTD_PCR4 & PORT_PCR_IRQC(1)) == 0);

        PORTD_PCR4 ^= PORT_PCR_IRQC(3);
    }

    panel_button = 0;
    encoder_button = 0;
}

__attribute__((interrupt ("IRQ"))) void portb_isr(void)
{
    PORTB_PCR16 |= PORT_PCR_ISF;

    /* The panel has been (de)pressed; reset the debouncing
     * timer. */

    if (!(PIT_TFLG(3) & PIT_TFLG_TIF)) {
        PIT_TCTRL(3) &= ~PIT_TCTRL_TEN;
        PIT_TCTRL(3) |= PIT_TCTRL_TEN;
    }

    panel_button = 1;
}

__attribute__((interrupt ("IRQ"))) void portd_isr(void)
{
    if (PORTD_ISFR & PT(4)) {
        PORTD_ISFR |= PT(4);

        /* The encoder button has been (de)pressed; reset the debouncing
         * timer. */

        if (!(PIT_TFLG(3) & PIT_TFLG_TIF)) {
            PIT_TCTRL(3) &= ~PIT_TCTRL_TEN;
            PIT_TCTRL(3) |= PIT_TCTRL_TEN;
        }

        encoder_button = 1;
    }

    if (!(PORTD_ISFR & (PT(2) | PT(3)))) {
        return;
    }

    PORTD_ISFR |= (PT(2) | PT(3));

    static unsigned int state;
    const unsigned int new = ~(GPIOD_PDIR >> 2) & 0b11;

    /* If an interrupt occured wihtin the debouncing interval,
     * reset the timer. */

    if ((PIT_TCTRL(2) & PIT_TCTRL_TEN)) {
        PIT_TCTRL(2) &= ~PIT_TCTRL_TEN;
        PIT_TCTRL(2) |= PIT_TCTRL_TEN;
    }

    /* If the new state does not constitute a valid transition, ignore
     * it. */

    const unsigned int d = state ^ new;
    if (d != 1 && d != 2) {
        return;
    }

    if (new == 3 && delta == 0) {
        /* We've arrived at the middle of the sequence.  If we haven't
         * established a rotation direction, it's safe to do so
         * now. */

        if (state == 2) {
            delta = -1;
        } else {
            uassert(state == 1);
            delta = 1;
        }
    } else if (new == 0 && delta != 0) {
        /* Transitions to state 0 complete a cycle so that
         * delta must be reset to 0. This makes debouncing
         * necessary, as bounces cannot be discarded reliably
         * with no assumptions as to the current rotation
         * direction.  Instead of resetting delta immediately,
         * we set a timeout to reset it. */

        PIT_TCTRL(2) |= PIT_TCTRL_TEN;
    } else {
        PIT_TCTRL(2) &= ~PIT_TCTRL_TEN;
    }

    state = new;
}
