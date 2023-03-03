#include "mk20dx.h"
#include "uassert.h"

extern void __libc_init_array (void);
extern void __libc_fini_array (void);
extern char __bss_start, __bss_end;
extern char __data_start, __data_end, __data_load;
extern int main (void);
extern unsigned long __stack_end;

static __attribute__ ((section(".flashconfig"), used))
const uint8_t flashconfigbytes[16] = {
    /* Backdoor comparison key */

    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,

    /* Program flash protection bytes */

    0xff, 0xff, 0xff, 0xff,

    /* FSEC */

    0x42,

    /* FOPT */

    0xf9,

    /* EEPROM protection byte */

    0xff,

    /* Data flash protection byte */

    0xff
};

static __attribute__((interrupt ("IRQ"))) void unused_isr(void)
{
    uassert(0);
}

static __attribute__((noreturn, interrupt ("IRQ"))) void fault_isr(void)
{
    uassert(0);
}

void pdb0_isr(void) __attribute__ ((weak, alias("unused_isr")));
void i2c0_isr(void) __attribute__ ((weak, alias("unused_isr")));
void ftm0_isr(void) __attribute__ ((weak, alias("unused_isr")));
void ftm1_isr(void) __attribute__ ((weak, alias("unused_isr")));
void pit0_isr(void) __attribute__ ((weak, alias("unused_isr")));
void pit1_isr(void) __attribute__ ((weak, alias("unused_isr")));
void pit2_isr(void) __attribute__ ((weak, alias("unused_isr")));
void pit3_isr(void) __attribute__ ((weak, alias("unused_isr")));
void porta_isr(void) __attribute__ ((weak, alias("unused_isr")));
void portb_isr(void) __attribute__ ((weak, alias("unused_isr")));
void portc_isr(void) __attribute__ ((weak, alias("unused_isr")));
void portd_isr(void) __attribute__ ((weak, alias("unused_isr")));
void usb_isr(void) __attribute__ ((weak, alias("unused_isr")));
void systick_isr(void) __attribute__ ((weak, alias("unused_isr")));

void reset(void)
{
    /* Unlock the watchdog and disable it.  After unlocking, we need
     * to wait 1 bus clock cycle before updating the status register
     * (see manual pg. 466).  After reset bus and core clocks are
     * equal, so this translates to 1 system clock cycle. */

    WDOG_UNLOCK = (uint16_t)0xc520;
    WDOG_UNLOCK = (uint16_t)0xd928;
    __asm__ volatile ("nop");
    WDOG_STCTRLH = WDOG_STCTRLH_ALLOWUPDATE;

    /* Reset on low voldage detection. */

    PMC_LVDSC1 |= PMC_LVDSC1_LVDRE;

    /* Configure the clock, see manual pg. 510. */

    OSC0_CR = OSC_SC8P | OSC_SC2P | OSC_ERCLKEN; /* Enable the crystal. */

#ifdef TEENSY30
    /* Select clock dividers for 48Mhz core, 48Mhz bus, 24 Mhz flash
     * clocks. */

    SIM_CLKDIV1 = SIM_CLKDIV1_OUTDIV1(0) | SIM_CLKDIV1_OUTDIV2(0) |
                  SIM_CLKDIV1_OUTDIV4(1);
#else
    /* Select clock dividers for 72Mhz core, 48Mhz bus, 24 Mhz flash
     * clocks.  See manual pg. 157-158 for maximum allowed
     * frequencsies. */

    SIM_CLKDIV1 = SIM_CLKDIV1_OUTDIV1(1) | SIM_CLKDIV1_OUTDIV2(2) |
                  SIM_CLKDIV1_OUTDIV4(5);
#endif

    /* Select the crystal as external references, very high frequency
     * range. */

    MCG_C2 = MCG_C2_RANGE0(2) | MCG_C2_EREFS0;

    /* Switch to external reference as clock source, FLL input = 16 MHz / 512 */

    MCG_C1 = MCG_C1_CLKS(2) | MCG_C1_FRDIV(4);

    /* Wait for crystal oscillator to initialize and sources to be
     * selected. */

    while (!(MCG_S & MCG_S_OSCINIT0));
    while (MCG_S & MCG_S_IREFST);
    while ((MCG_S & MCG_S_CLKST_MASK) != MCG_S_CLKST(2));

    /* Now in FBE mode, clocked by the crystal at 16Mhz with the FLL
     * bypassed. */

#ifdef TEENSY30
    /* Configure the PLL divider for 16 MHz / 8 = 2 MHz and the output
     * multiplier for 48 Mhz. */

    MCG_C5 = MCG_C5_PRDIV0(7);
    MCG_C6 = MCG_C6_PLLS | MCG_C6_VDIV0(0);
#else
    /* Configure the PLL divider for 16 MHz / 5 = 3.2 MHz and the
     * output multiplier for 144 Mhz. */

    MCG_C5 = MCG_C5_PRDIV0(4);
    MCG_C6 = MCG_C6_PLLS | MCG_C6_VDIV0(21);
#endif

    /* Wait for PLL lock and source selection and we're in PBE
     * mode. */

    while (!(MCG_S & MCG_S_PLLST));
    while (!(MCG_S & MCG_S_LOCK0));

    /* Switch to PLL as clock source, wait for PLL clock to be
     * selected and we're in PEE mode. */

    MCG_C1 = MCG_C1_CLKS(0) | MCG_C1_FRDIV(4);
    while ((MCG_S & MCG_S_CLKST_MASK) != MCG_S_CLKST(3));

    /* Copy the data section to RAM and clear the bss section. */

    for (char *c = &__bss_start; c < &__bss_end; *c++ = 0);
    for (char *c = &__data_start, *d = &__data_load;
         c < &__data_end;
         *c++ = *d++);

    /* Configure the LED pin as an output. */

    SIM_SCGC5 |= SIM_SCGC5_PORTC;
    PORTC_PCR5 = PORT_PCR_MUX(1);
    GPIOC_PDDR |= PT(5);

    __libc_init_array();

    main();

    __libc_fini_array();

    while(1);
}

__attribute__ ((section(".vectors"), used))
void (* const vectors[])(void) =
{
    (void (*)(void))&__stack_end, /* ARM core Initial Stack Pointer */
    reset,   /* ARM core Initial Program Counter */
    unused_isr, /* ARM core Non-maskable Interrupt (NMI) */
    fault_isr, /* ARM core Hard Fault */
    fault_isr, /* ARM core MemManage Fault */
    fault_isr, /* ARM core Bus Fault */
    fault_isr, /* ARM core Usage Fault */
    unused_isr, /* - - */
    unused_isr, /* - - */
    unused_isr, /* - - */
    unused_isr, /* - - */
    unused_isr, /* ARM core Supervisor call (SVCall) */
    unused_isr, /* ARM core Debug Monitor */
    unused_isr, /* - - */
    unused_isr, /* ARM core Pendable request for system service */
    systick_isr, /* ARM core System tick timer (SysTick) */
    unused_isr, /* DMA DMA channel 0 transfer complete */
    unused_isr, /* DMA DMA channel 1 transfer complete */
    unused_isr, /* DMA DMA channel 2 transfer complete */
    unused_isr, /* DMA DMA channel 3 transfer complete */

#ifndef TEENSY30
    unused_isr, /* DMA DMA channel 4 transfer complete */
    unused_isr, /* DMA DMA channel 5 transfer complete */
    unused_isr, /* DMA DMA channel 6 transfer complete */
    unused_isr, /* DMA DMA channel 7 transfer complete */
    unused_isr, /* DMA DMA channel 8 transfer complete */
    unused_isr, /* DMA DMA channel 9 transfer complete */
    unused_isr, /* DMA DMA channel 10 transfer complete */
    unused_isr, /* DMA DMA channel 11 transfer complete */
    unused_isr, /* DMA DMA channel 12 transfer complete */
    unused_isr, /* DMA DMA channel 13 transfer complete */
    unused_isr, /* DMA DMA channel 14 transfer complete */
    unused_isr, /* DMA DMA channel 15 transfer complete */
#endif

    unused_isr, /* DMA DMA error interrupt channels 0-15 */
    unused_isr, /* - - */
    unused_isr, /* Flash memory Command complete */
    unused_isr, /* Flash memory Read collision */
    unused_isr, /* Mode Controller Low-voltage detect, low-voltage warning */
    unused_isr, /* LLWU Low Leakage Wakeup */
    unused_isr, /* WDOG or EWM */

#ifndef TEENSY30
    unused_isr, /* - - */
#endif

    i2c0_isr, /* I2C0 - */

#ifndef TEENSY30
    unused_isr, /* I2C1 - */
#endif

    unused_isr, /* SPI0 Single interrupt vector for all sources */

#ifndef TEENSY30
    unused_isr, /* SPI1 Single interrupt vector for all sources */
    unused_isr, /* - - */
    unused_isr, /* CAN0 OR'ed Message buffer (0-15) */
    unused_isr, /* CAN0 Bus Off */
    unused_isr, /* CAN0 Error */
    unused_isr, /* CAN0 Transmit Warning */
    unused_isr, /* CAN0 Receive Warning */
    unused_isr, /* CAN0 Wake Up */
#endif

    unused_isr, /* I2S0 Transmit */
    unused_isr, /* I2S0 Receive */

#ifndef TEENSY30
    unused_isr, /* - - */
    unused_isr, /* - - */
    unused_isr, /* - - */
    unused_isr, /* - - */
    unused_isr, /* - - */
    unused_isr, /* - - */
    unused_isr, /* - - */
#endif

    unused_isr, /* UART0 Single interrupt vector for UART LON sources */
    unused_isr, /* UART0 Single interrupt vector for UART status sources */
    unused_isr, /* UART0 Single interrupt vector for UART error sources */
    unused_isr, /* UART1 Single interrupt vector for UART status sources */
    unused_isr, /* UART1 Single interrupt vector for UART error sources */
    unused_isr, /* UART2 Single interrupt vector for UART status sources */
    unused_isr, /* UART2 Single interrupt vector for UART error sources */

#ifndef TEENSY30
    unused_isr, /* */
    unused_isr, /* */
    unused_isr, /* */
    unused_isr, /* */
    unused_isr, /* */
    unused_isr, /* */
#endif

    unused_isr, /* ADC0 - */

#ifndef TEENSY30
    unused_isr, /* ADC1 - */
#endif

    unused_isr, /* CMP0 - */
    unused_isr, /* CMP1 - */

#ifndef TEENSY30
    unused_isr, /* CMP2 - */
#endif

    ftm0_isr, /* FTM0 Single interrupt vector for all sources */
    ftm1_isr, /* FTM1 Single interrupt vector for all sources */

#ifndef TEENSY30
    unused_isr, /* FTM2 Single interrupt vector for all sources */
#endif

    unused_isr, /* CMT - */
    unused_isr, /* RTC Alarm interrupt */
    unused_isr, /* RTC Seconds interrupt */
    pit0_isr, /* PIT Channel 0 */
    pit1_isr, /* PIT Channel 1 */
    pit2_isr, /* PIT Channel 2 */
    pit3_isr, /* PIT Channel 3 */
    pdb0_isr, /* PDB - */
    usb_isr, /* USB OTG - */
    unused_isr, /* USB Charger Detect - */

#ifndef TEENSY30
    unused_isr, /* - - */
    unused_isr, /* - - */
    unused_isr, /* - - */
    unused_isr, /* - - */
    unused_isr, /* - - */
    unused_isr, /* - - */
    unused_isr, /* DAC0 - */
    unused_isr, /* - - */
#endif

    unused_isr, /* TSI Single interrupt vector for all sources */
    unused_isr, /* MCG - */
    unused_isr, /* Low Power Timer - */

#ifndef TEENSY30
    unused_isr, /* - - */
#endif

    porta_isr, /* Port control module Pin detect (Port A) */
    portb_isr, /* Port control module Pin detect (Port B) */
    portc_isr, /* Port control module Pin detect (Port C) */
    portd_isr, /* Port control module Pin detect (Port D) */
    unused_isr, /* Port control module Pin detect (Port E) */

#ifndef TEENSY30
    unused_isr, /* - - */
    unused_isr, /* - - */
#endif

    unused_isr, /* Software Software interrupt */
};
