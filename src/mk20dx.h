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

#ifndef MK20DX_H
#define MK20DX_H

#include <inttypes.h>

#define toggle_led() GPIOC_PTOR = PT(5)
#define set_led() GPIOC_PSOR = PT(5)
#define clear_led() GPIOC_PCOR = PT(5)

#define SCB_AIRCR (*(volatile uint32_t *)0xE000ED0C)
#define PMC_LVDSC1 (*(volatile uint8_t  *)0x4007D000)
#define PMC_LVDSC1_LVDRE ((uint8_t)1 << 4)

#define FTFL_FSTAT (*(volatile uint8_t *)0x40020000)
#define FTFL_FSTAT_CCIF ((uint8_t)1 << 7)
#define FTFL_FSTAT_ACCERR ((uint8_t)1 << 5)
#define FTFL_FSTAT_FPVIOL ((uint8_t)1 << 4)
#define FTFL_FSTAT_MGSTAT0 ((uint8_t)1 << 1)
#define FTFL_FCNFG (*(volatile uint8_t *)0x40020001)
#define FTFL_FCNFG_RDCOLLIE ((uint8_t)1 << 6)
#define FTFL_FCNFG_RAMRDY ((uint8_t)1 << 1)
#define FTFL_FCCOB3 (*(volatile uint8_t *)0x40020004)
#define FTFL_FCCOB2 (*(volatile uint8_t *)0x40020005)
#define FTFL_FCCOB1 (*(volatile uint8_t *)0x40020006)
#define FTFL_FCCOB0 (*(volatile uint8_t *)0x40020007)
#define FTFL_FCCOB7 (*(volatile uint8_t *)0x40020008)
#define FTFL_FCCOB6 (*(volatile uint8_t *)0x40020009)
#define FTFL_FCCOB5 (*(volatile uint8_t *)0x4002000A)
#define FTFL_FCCOB4 (*(volatile uint8_t *)0x4002000B)
#define FTFL_FCCOBB (*(volatile uint8_t *)0x4002000C)
#define FTFL_FCCOBA (*(volatile uint8_t *)0x4002000D)
#define FTFL_FCCOB9 (*(volatile uint8_t *)0x4002000E)
#define FTFL_FCCOB8 (*(volatile uint8_t *)0x4002000F)

#define NVIC_ISER(n) (*((volatile uint32_t *)0xe000e100 + n))
#define NVIC_ICER(n) (*((volatile uint32_t *)0xe000e180 + n))
#define NVIC_ISPR(n) (*((volatile uint32_t *)0xe000e200 + n))
#define NVIC_ICPR(n) (*((volatile uint32_t *)0xe000e280 + n))
#define NVIC_IPR(n)  (*((volatile uint32_t *)0xe000e400 + n))

#define enable_interrupt(n) (NVIC_ISER((n) / 32) = ((uint32_t)1 << ((n) % 32)))
#define disable_interrupt(n) (NVIC_ICER((n) / 32) = ((uint32_t)1 << ((n) % 32)))
#define pend_interrupt(n) (NVIC_ISPR((n) / 32) = ((uint32_t)1 << ((n) % 32)))
#define unpend_interrupt(n) (NVIC_ICPR((n) / 32) = ((uint32_t)1 << ((n) % 32)))
#define is_interrupt_enabled(n) (NVIC_ISER((n) / 32) & ((uint32_t)1 << ((n) % 32)))
#define is_interrupt_pending(n) (NVIC_ISPR((n) / 32) & ((uint32_t)1 << ((n) % 32)))
#define prioritize_interrupt(n, p)                                      \
    {                                                                   \
        int i = n / 4, j = 8 * (n % 4) + 4;                             \
                                                                        \
        NVIC_IPR(i) = ((NVIC_IPR(i) & ~((uint32_t)0xf << j)) |          \
                       ((p & 0xf) << j));                               \
    }

#define disable_interrupts() asm("cpsid i")
#define enable_interrupts() asm("cpsie i")

#define disable_all_interrupts()                                \
    {                                                           \
        NVIC_ICER(0) = ~(uint32_t)1;                            \
        NVIC_ICER(1) = ~(uint32_t)1;                            \
    }

#define OSC0_CR (*((volatile uint8_t *)0x40065000))
#define OSC_SC8P ((uint8_t)1 << 1)
#define OSC_SC2P ((uint8_t)1 << 3)
#define OSC_ERCLKEN ((uint8_t)1 << 7)

#define MCG_C1 (*((volatile uint8_t *)0x40064000))
#define MCG_C2 (*((volatile uint8_t *)0x40064001))
#define MCG_C5 (*((volatile uint8_t *)0x40064004))
#define MCG_C6 (*((volatile uint8_t *)0x40064005))
#define MCG_S (*((volatile uint8_t *)0x40064006))
#define MCG_C1_CLKS(n) (((uint8_t)(n) & 0b11) << 6)
#define MCG_C1_FRDIV(n) (((uint8_t)(n) & 0b111) << 3)
#define MCG_C1_IRCLKEN ((uint8_t)1 << 1)
#define MCG_C2_RANGE0(n) (((uint8_t)(n) & 0b11) << 4)
#define MCG_C2_EREFS0 ((uint8_t)1 << 2)
#define MCG_C5_PRDIV0(n)(uint8_t) ((n) & 0b11111)
#define MCG_C6_PLLS ((uint8_t)1 << 6)
#define MCG_C6_VDIV0(n)(uint8_t) ((n) & 0b11111)
#define MCG_S_OSCINIT0 ((uint8_t)1 << 1)
#define MCG_S_IREFST ((uint8_t)1 << 4)
#define MCG_S_CLKST_MASK (uint8_t)(0b11UL << 2)
#define MCG_S_CLKST(n) (((uint8_t)(n) & 0b11) << 2)
#define MCG_S_PLLST ((uint8_t)1 << 5)
#define MCG_S_LOCK0 ((uint8_t)1 << 6)

#define WDOG_STCTRLH (*(volatile uint16_t *)0x40052000)
#define WDOG_UNLOCK (*(volatile uint16_t *)0x4005200e)
#define WDOG_STCTRLH_ALLOWUPDATE ((uint16_t)1 << 4)

#define SYST_CSR (*(volatile uint32_t *)0xe000e010)
#define SYST_RVR (*(volatile uint32_t *)0xe000e014)
#define SYST_CVR (*(volatile uint32_t *)0xe000e018)
#define SYST_CSR_COUNTFLAG ((uint32_t)1 << 16)
#define SYST_CSR_CLKSOURCE ((uint32_t)1 << 2)
#define SYST_CSR_TICKINT ((uint32_t)1 << 1)
#define SYST_CSR_ENABLE ((uint32_t)1 << 0)

#define SIM_CLKDIV1 (*((volatile uint32_t *)0x40048044))
#define SIM_CLKDIV2 (*((volatile uint32_t *)0x40048048))
#define SIM_CLKDIV1_OUTDIV1(n) (((uint32_t)(n) & 0b1111) << 28)
#define SIM_CLKDIV1_OUTDIV2(n) (((uint32_t)(n) & 0b1111) << 24)
#define SIM_CLKDIV1_OUTDIV4(n) (((uint32_t)(n) & 0b1111) << 16)
#define SIM_CLKDIV2_USBDIV(n) (((uint32_t)(n) & 0b111) << 1)
#define SIM_CLKDIV2_USBFRAC ((uint32_t)1 << 0)
#define SIM_SOPT2 (*((volatile uint32_t *)0x40048004))
#define SIM_SOPT2_USBSRC ((uint32_t)1 << 18)
#define SIM_SOPT2_PLLFLLSEL ((uint32_t)1 << 16)
#define SIM_SOPT2_TRACECLKSEL ((uint32_t)1 << 12)

#define SIM_SCGC4 (*((volatile uint32_t *)0x40048034))
#define SIM_SCGC4_I2C0 ((uint32_t)1 << 6)
#define SIM_SCGC4_USBOTG ((uint32_t)1 << 18)
#define SIM_SCGC5 (*((volatile uint32_t *)0x40048038))
#define SIM_SCGC5_PORTA ((uint32_t)1 << 9)
#define SIM_SCGC5_PORTB ((uint32_t)1 << 10)
#define SIM_SCGC5_PORTC ((uint32_t)1 << 11)
#define SIM_SCGC5_PORTD ((uint32_t)1 << 12)
#define SIM_SCGC5_PORTE ((uint32_t)1 << 13)

#define SIM_SCGC6 (*(volatile uint32_t *)0x4004803C)
#define SIM_SCGC6_SPI0 ((uint32_t)1 << 12)
#define SIM_SCGC6_PDB ((uint32_t)1 << 22)
#define SIM_SCGC6_PIT ((uint32_t)1 << 23)
#define SIM_SCGC6_FTM0 ((uint32_t)1 << 24)
#define SIM_SCGC6_FTM1 ((uint32_t)1 << 25)

#ifdef TEENSY30
#define PIT0_IRQ 30
#define PIT1_IRQ 31
#define PIT2_IRQ 32
#define PIT3_IRQ 33
#else
#define PIT0_IRQ 68
#define PIT1_IRQ 69
#define PIT2_IRQ 70
#define PIT3_IRQ 71
#endif

#define PIT_MCR *(volatile uint32_t *)0x40037000
#define PIT_LDVAL(n) (*((volatile uint32_t *)(0x40037100 + 0x10 * (n))))
#define PIT_CVAL(n) (*((volatile uint32_t *)(0x40037104 + 0x10 * (n))))
#define PIT_TCTRL(n) (*((volatile uint32_t *)(0x40037108 + 0x10 * (n))))
#define PIT_TFLG(n) (*((volatile uint32_t *)(0x4003710C + 0x10 * (n))))
#define PIT_MCR_MDIS ((uint32_t)1 << 1)
#define PIT_TCTRL_TEN ((uint32_t)1 << 0)
#define PIT_TCTRL_TIE ((uint32_t)1 << 1)
#define PIT_TFLG_TIF ((uint32_t)1 << 0)

#ifdef TEENSY30
#define USB0_IRQ 35
#else
#define USB0_IRQ 73
#endif

#define USB0_STAT (*((volatile uint8_t *)0x40072090))
#define USB0_INTEN (*((volatile uint8_t *)0x40072084))
#define USB0_ERREN (*((volatile uint8_t *)0x4007208c))
#define USB0_OTGISTAT (*((volatile uint8_t *)0x40072010))
#define USB0_ISTAT (*((volatile uint8_t *)0x40072080))
#define USB0_ERRSTAT (*((volatile uint8_t *)0x40072088))
#define USB0_CONTROL (*((volatile uint8_t *)0x40072108))
#define USB0_CTL (*((volatile uint8_t *)0x40072094))
#define USB0_USBCTRL (*((volatile uint8_t *)0x40072100))
#define USB0_BDTPAGE1 (*((volatile uint8_t *)0x4007209c))
#define USB0_BDTPAGE2 (*((volatile uint8_t *)0x400720b0))
#define USB0_BDTPAGE3 (*((volatile uint8_t *)0x400720b4))
#define USB0_USBTRC0 (*((volatile uint8_t *)0x4007210c))
#define USB0_ENDPT(n) (*((volatile uint8_t *)(0x400720c0 + 4 * (n))))
#define USB0_ADDR (*((volatile uint8_t *)0x40072098))
#define USB_USBTRC0_USBRESET ((uint8_t)1 << 7)
#define USB_CTL_TXSUSPENDTOKENBUSY ((uint8_t)1 << 5)
#define USB_CTL_USBENSOFEN ((uint8_t)1 << 0)
#define USB_CTL_ODDRST ((uint8_t)1 << 1)
#define USB_USBCTRL_SUSP ((uint8_t)1 << 7)
#define USB_USBCTRL_PDE ((uint8_t)1 << 6)
#define USB_CONTROL_DPPULLUPNONOTG ((uint8_t)1 << 4)
#define USB_ISTAT_STALL ((uint8_t)1 << 7)
#define USB_ISTAT_SLEEP ((uint8_t)1 << 4)
#define USB_ISTAT_TOKDNE ((uint8_t)1 << 3)
#define USB_ISTAT_SOFTOK ((uint8_t)1 << 2)
#define USB_ISTAT_ERROR ((uint8_t)1 << 1)
#define USB_ISTAT_USBRST ((uint8_t)1 << 0)
#define USB_ERRSTAT_BTOERR ((uint8_t)1 << 4)
#define USB_ENDPT_EPRXEN ((uint8_t)1 << 3)
#define USB_ENDPT_EPTXEN ((uint8_t)1 << 2)
#define USB_ENDPT_EPSTALL ((uint8_t)1 << 1)
#define USB_ENDPT_EPHSHK ((uint8_t)1 << 0)
#define USB_INTEN_STALLEN ((uint8_t)1 << 7)
#define USB_INTEN_SLEEPEN ((uint8_t)1 << 4)
#define USB_INTEN_TOKDNEEN ((uint8_t)1 << 3)
#define USB_INTEN_SOFTOKEN ((uint8_t)1 << 2)
#define USB_INTEN_ERROREN ((uint8_t)1 << 1)
#define USB_INTEN_USBRSTEN ((uint8_t)1 << 0)
#define USB_STAT_TX ((uint8_t)1 << 3)
#define USB_STAT_ODD ((uint8_t)1 << 2)
#define USB_STAT_ENDP(n) (uint8_t)((n) >> 4)

#define PT(n) ((uint32_t)1 << (n))
#define PORT_PCR_MUX(n) (((uint32_t)(n) & 0b111) << 8)
#define PORT_PCR_IRQC(n) (((uint32_t)(n) & 0b1111) << 16)
#define PORT_PCR_ISF ((uint32_t)1 << 24)
#define PORT_PCR_DSE ((uint32_t)1 << 6)
#define PORT_PCR_ODE ((uint32_t)1 << 5)
#define PORT_PCR_PFE ((uint32_t)1 << 4)
#define PORT_PCR_SRE ((uint32_t)1 << 2)
#define PORT_PCR_PE ((uint32_t)1 << 1)
#define PORT_PCR_PS ((uint32_t)1 << 0)

#ifdef TEENSY30
#define PORTA_IRQ 40
#define PORTB_IRQ 41
#define PORTC_IRQ 42
#define PORTD_IRQ 43
#else
#define PORTA_IRQ 87
#define PORTB_IRQ 88
#define PORTC_IRQ 89
#define PORTD_IRQ 90
#endif

#define PORTA_PCR12 (*(volatile uint32_t *)0x40049030)

#define PORTB_PCR0 (*((volatile uint32_t *)0x4004a000))
#define PORTB_PCR1 (*((volatile uint32_t *)0x4004a004))
#define PORTB_PCR2 (*((volatile uint32_t *)0x4004a008))
#define PORTB_PCR3 (*((volatile uint32_t *)0x4004a00c))
#define PORTB_PCR16 (*((volatile uint32_t *)0x4004a040))
#define PORTB_PCR17 (*((volatile uint32_t *)0x4004a044))
#define PORTB_ISFR (*((volatile uint32_t *)0x4004a0a0))

#define PORTC_PCR0 (*((volatile uint32_t *)0x4004b000))
#define PORTC_PCR1 (*((volatile uint32_t *)0x4004b004))
#define PORTC_PCR2 (*((volatile uint32_t *)0x4004b008))
#define PORTC_PCR3 (*((volatile uint32_t *)0x4004b00c))
#define PORTC_PCR4 (*((volatile uint32_t *)0x4004b010))
#define PORTC_PCR5 (*((volatile uint32_t *)0x4004b014))
#define PORTC_PCR6 (*((volatile uint32_t *)0x4004b018))
#define PORTC_PCR7 (*((volatile uint32_t *)0x4004b01c))
#define PORTC_ISFR (*((volatile uint32_t *)0x4004b0a0))

#define PORTD_PCR0 (*((volatile uint32_t *)0x4004c000))
#define PORTD_PCR1 (*((volatile uint32_t *)0x4004c004))
#define PORTD_PCR2 (*((volatile uint32_t *)0x4004c008))
#define PORTD_PCR3 (*((volatile uint32_t *)0x4004c00c))
#define PORTD_PCR4 (*((volatile uint32_t *)0x4004c010))
#define PORTD_PCR5 (*((volatile uint32_t *)0x4004c014))
#define PORTD_PCR6 (*((volatile uint32_t *)0x4004c018))
#define PORTD_ISFR (*((volatile uint32_t *)0x4004c0a0))

#define GPIOB_PDOR (*((volatile uint32_t *)0x400ff040))
#define GPIOB_PSOR (*((volatile uint32_t *)0x400ff044))
#define GPIOB_PCOR (*((volatile uint32_t *)0x400ff048))
#define GPIOB_PTOR (*((volatile uint32_t *)0x400ff04c))
#define GPIOB_PDIR (*((volatile uint32_t *)0x400ff050))
#define GPIOB_PDDR (*((volatile uint32_t *)0x400ff054))

#define GPIOC_PDOR (*((volatile uint32_t *)0x400ff080))
#define GPIOC_PSOR (*((volatile uint32_t *)0x400ff084))
#define GPIOC_PCOR (*((volatile uint32_t *)0x400ff088))
#define GPIOC_PTOR (*((volatile uint32_t *)0x400ff08c))
#define GPIOC_PDIR (*((volatile uint32_t *)0x400ff090))
#define GPIOC_PDDR (*((volatile uint32_t *)0x400ff094))

#define GPIOD_PDOR (*((volatile uint32_t *)0x400ff0c0))
#define GPIOD_PSOR (*((volatile uint32_t *)0x400ff0c4))
#define GPIOD_PCOR (*((volatile uint32_t *)0x400ff0c8))
#define GPIOD_PTOR (*((volatile uint32_t *)0x400ff0cc))
#define GPIOD_PDIR (*((volatile uint32_t *)0x400ff0d0))
#define GPIOD_PDDR (*((volatile uint32_t *)0x400ff0d4))

#ifdef TEENSY30
#define FTM0_IRQ 25
#define FTM1_IRQ 26
#else
#define FTM0_IRQ 62
#define FTM1_IRQ 63
#endif

#define FTM0_SC (*(volatile uint32_t *)0x40038000)
#define FTM0_CNT (*(volatile uint32_t *)0x40038004)
#define FTM0_MOD (*(volatile uint32_t *)0x40038008)
#define FTM0_C0SC (*(volatile uint32_t *)0x4003800c)
#define FTM0_C0V (*(volatile uint32_t *)0x40038010)
#define FTM0_C1SC (*(volatile uint32_t *)0x40038014)
#define FTM0_C1V (*(volatile uint32_t *)0x40038018)
#define FTM0_CNTIN (*(volatile uint32_t *)0x4003804c)
#define FTM0_MODE (*(volatile uint32_t *)0x40038054)
#define FTM0_COMBINE (*(volatile uint32_t *)0x40038064)
#define FTM0_FILTER (*(volatile uint32_t *)0x40038078)

#define FTM1_SC (*(volatile uint32_t *)0x40039000)
#define FTM1_CNT (*(volatile uint32_t *)0x40039004)
#define FTM1_MOD (*(volatile uint32_t *)0x40039008)
#define FTM1_C0SC (*(volatile uint32_t *)0x4003900c)
#define FTM1_C0V (*(volatile uint32_t *)0x40039010)
#define FTM1_CNTIN (*(volatile uint32_t *)0x4003904c)
#define FTM1_MODE (*(volatile uint32_t *)0x40039054)

#define FTM_COMBINE_DECAPEN0 ((uint32_t)1 << 2)
#define FTM_COMBINE_DECAP0 ((uint32_t)1 << 3)

#define FTM_CSC_ELSA ((uint32_t)1 << 2)
#define FTM_CSC_ELSB ((uint32_t)1 << 3)
#define FTM_CSC_MSA ((uint32_t)1 << 4)
#define FTM_CSC_MSB ((uint32_t)1 << 5)
#define FTM_CSC_CHIE ((uint32_t)1 << 6)
#define FTM_CSC_CHF ((uint32_t)1 << 7)

#define FTM_MODE_WPDIS ((uint32_t)1 << 2)
#define FTM_MODE_FTMEN ((uint32_t)1 << 0)

#define FTM_FILTER_CH0FVAL(n) (((uint32_t)(n) & 0b1111) << 0)
#define FTM_FILTER_CH1FVAL(n) (((uint32_t)(n) & 0b1111) << 4)

#define FTM_SC_CLKS(n) (((uint32_t)(n) & 0b11) << 3)
#define FTM_SC_PS(n) ((uint32_t)(n) & 0b111)
#define FTM_SC_CPWMS ((uint32_t)1 << 5)
#define FTM_SC_TOIE ((uint32_t)1 << 6)
#define FTM_SC_TOF ((uint32_t)1 << 7)

#define SPI0_MCR (*((volatile uint32_t *)0x4002c000))
#define SPI0_CTAR0 (*((volatile uint32_t *)0x4002c00c))
#define SPI0_CTAR1 (*((volatile uint32_t *)0x4002c010))
#define SPI0_SR (*((volatile uint32_t *)0x4002c02c))
#define SPI0_PUSHR (*((volatile uint32_t *)0x4002c034))
#define SPI0_POPR (*((volatile uint32_t *)0x4002c038))
#define SPI_MCR_HALT ((uint32_t)1 << 1)
#define SPI_MCR_CLR_RXF ((uint32_t)1 << 10)
#define SPI_MCR_CLR_TXF ((uint32_t)1 << 11)
#define SPI_MCR_DIS_RXF ((uint32_t)1 << 12)
#define SPI_MCR_DIS_TXF ((uint32_t)1 << 13)
#define SPI_MCR_MDIS ((uint32_t)1 << 14)
#define SPI_MCR_ROOE ((uint32_t)1 << 24)
#define SPI_MCR_PCSIS(n) ((((uint32_t)1 << (n + 16)) & (0b111111 << 16)))
#define SPI_MCR_MSTR ((uint32_t)1 << 31)
#define SPI_CTAR_CPHA ((uint32_t)1 << 25)
#define SPI_CTAR_FMSZ(n) (((uint32_t)(n) & 0b1111) << 27)
#define SPI_CTAR_PCSSCK(n) (((uint32_t)(n) & 0b11) << 22)
#define SPI_CTAR_CSSCK(n) (((uint32_t)(n) & 0b1111) << 12)
#define SPI_CTAR_PASC(n) (((uint32_t)(n) & 0b11) << 20)
#define SPI_CTAR_ASC(n) (((uint32_t)(n) & 0b1111) << 8)
#define SPI_CTAR_PDT(n) (((uint32_t)(n) & 0b11) << 18)
#define SPI_CTAR_DT(n) (((uint32_t)(n) & 0b1111) << 4)
#define SPI_CTAR_PBR(n) (((uint32_t)(n) & 0b11) << 16)
#define SPI_CTAR_BR(n) (((uint32_t)(n) & 0b1111) << 0)
#define SPI_PUSHR_PCS(n) SPI_MCR_PCSIS(n)
#define SPI_PUSHR_EOQ ((uint32_t)1 << 27)
#define SPI_PUSHR_CTAS(n) (((uint32_t)(n) & 0b111) << 28)
#define SPI_PUSHR_CONT ((uint32_t)1 << 31)
#define SPI_SR_RXCTR_OFFSET 4
#define SPI_SR_RXCTR_MASK (((uint32_t)0b1111) << SPI_SR_RXCTR_OFFSET)
#define SPI_SR_TXCTR_OFFET 12
#define SPI_SR_TXCTR_MASK (((uint32_t)0b1111) << SPI_SR_TXCTR_OFFET)
#define SPI_SR_RFDF ((uint32_t)1 << 17)
#define SPI_SR_RFOF ((uint32_t)1 << 19)
#define SPI_SR_TFFF ((uint32_t)1 << 25)
#define SPI_SR_EOQF ((uint32_t)1 << 28)
#define SPI_SR_TCF ((uint32_t)1 << 31)

#ifdef TEENSY30
#define I2C0_IRQ 11
#else
#define I2C0_IRQ 24
#endif

#define I2C0_A1 *(volatile uint8_t *)0x40066000
#define I2C0_F *(volatile uint8_t *)0x40066001
#define I2C0_C1 *(volatile uint8_t *)0x40066002
#define I2C0_S *(volatile uint8_t *)0x40066003
#define I2C0_D *(volatile uint8_t *)0x40066004
#define I2C0_C2 *(volatile uint8_t *)0x40066005
#define I2C0_FLT *(volatile uint8_t *)0x40066006
#define I2C_F_MULT(n) (((uint8_t)(n) & 0b11) << 6)
#define I2C_F_ICR(n) (((uint8_t)(n) & 0b111111) << 0)
#define I2C_C1_IICEN ((uint8_t)1 << 7)
#define I2C_C1_IICIE ((uint8_t)1 << 6)
#define I2C_C1_MST ((uint8_t)1 << 5)
#define I2C_C1_TX ((uint8_t)1 << 4)
#define I2C_C1_TXAK ((uint8_t)1 << 3)
#define I2C_C1_RSTA ((uint8_t)1 << 2)
#define I2C_S_TCF ((uint8_t)1 << 7)
#define I2C_S_BUSY ((uint8_t)1 << 5)
#define I2C_S_ARBL ((uint8_t)1 << 4)
#define I2C_S_IICIF ((uint8_t)1 << 1)
#define I2C_S_RXAK ((uint8_t)1 << 0)
#define I2C_C2_HDRS ((uint8_t)1 << 5)
#define I2C_FLT_FLT(n) (uint8_t)((n) & 0b1111)

#ifdef TEENSY30
#define PDB0_IRQ 34
#else
#define PDB0_IRQ 72
#endif

#define PDB0_SC (*(volatile uint32_t *)0x40036000)
#define PDB0_CNT (*(volatile uint32_t *)0x40036008)
#define PDB0_IDLY (*(volatile uint32_t *)0x4003600c)
#define PDB_SC_LDOK ((uint32_t)1 << 0)
#define PDB_SC_MULT(n) (((n) & 0b11) << 2)
#define PDB_SC_PDBIE ((uint32_t)1 << 5)
#define PDB_SC_PDBIF ((uint32_t)1 << 6)
#define PDB_SC_PDBEN ((uint32_t)1 << 7)
#define PDB_SC_TRGSEL(n) (((n) & 0b1111) << 8)
#define PDB_SC_PRESCALER(n) (((n) & 0b111) << 12)
#define PDB_SC_SWTRIG ((uint32_t)1 << 16)

#endif
