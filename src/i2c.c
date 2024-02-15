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
#include <stddef.h>

#include "callbacks.h"
#include "filter.h"
#include "mk20dx.h"
#include "time.h"
#include "uassert.h"
#include "usb.h"

enum slave {
    NAU7802=0x54,
    NSA2862X=0xda,
};

#define WAIT()                                  \
    WAIT_WHILE(!(I2C0_S & I2C_S_IICIF), 50);    \
    I2C0_S |= I2C_S_IICIF;

#define READ() I2C0_D; WAIT()

#define WRITE(b)                                                        \
    {                                                                   \
        I2C0_D = b;                                                     \
        WAIT();                                                         \
                                                                        \
        if (I2C0_S & I2C_S_RXAK || I2C0_S & I2C_S_ARBL) {               \
            uprintf(                                                    \
                "I2C error (" __FILE__ ":%d, S: %b, C1: %b)\n",         \
                __LINE__, I2C0_S, I2C0_C1);                             \
            I2C0_S |= I2C_S_ARBL;                                       \
            goto error;                                                 \
        }                                                               \
    }

struct filter pressure_filter = DOUBLE_FILTER(0.12, 60.0);
struct filter mass_filter = SINGLE_FILTER(0.1);

static struct {
    enum slave slave;
    uint8_t *buffer, length, phase, reg, value;
} context;

static bool run[2];
static int taring_state = 1;

static void read_noblock(uint8_t slave, uint8_t reg, size_t n)
{
    static uint8_t b[3];

    context.phase = 0;
    context.slave = slave;
    context.reg = reg;
    context.buffer = b;
    context.length = n;

    I2C0_S |= I2C_S_IICIF;
    I2C0_C1 |= (I2C_C1_MST | I2C_C1_TX | I2C_C1_IICIE);
    I2C0_D = slave | 0;
}

#if 0
static void write_noblock(uint8_t slave, uint8_t reg, uint8_t x)
{
    context.slave = slave;
    context.reg = reg;
    context.buffer = NULL;
    context.length = 0;
    context.value = x;

    I2C0_S |= I2C_S_IICIF;
    I2C0_C1 |= (I2C_C1_MST | I2C_C1_TX | I2C_C1_IICIE);
    I2C0_D = slave | 0;
}
#endif

void read_pressure(void)
{
    if (run[0] || run[1]) {
        return;
    }

    read_noblock(NSA2862X, 0x2, 1);
}

void run_pressure(bool r)
{
    if (run[0] != r && r) {
        PDB0_SC |= PDB_SC_SWTRIG;
    }

    run[0] = r;
}

void read_mass(void)
{
    if (run[0] || run[1]) {
        return;
    }

    read_noblock(NAU7802, 0x0, 1);
}

void run_mass(bool r)
{
    if (run[1] != r && r) {
        PDB0_SC |= PDB_SC_SWTRIG;
    }

    run[1] = r;
}

void tare_mass(void)
{
    taring_state = 1;
}

bool is_taring_mass(void)
{
    return taring_state > 0;
}

__attribute__((interrupt ("IRQ"))) void pdb0_isr(void)
{
    PDB0_SC &= ~PDB_SC_PDBIF;

    const double t = get_time();

    if (t - pressure_filter.t > 0.1) {
        filter_sample(&pressure_filter, NAN, get_time());
    }

    if (t - mass_filter.t > 1) {
        filter_sample(&mass_filter, NAN, get_time());
    }

    if (!(I2C0_C1 & I2C_C1_IICEN)) {
        PORTB_PCR2 = PORTB_PCR3 = (
            PORT_PCR_MUX(2) | PORT_PCR_ODE | PORT_PCR_DSE);
        I2C0_C1 |= I2C_C1_IICEN;
    } else if (I2C0_C1 & I2C_C1_MST) {
        uprintf("I2C module in master mode (S: %b, C1: %b)\n", I2C0_S, I2C0_C1);

        I2C0_C1 = 0;
    } else if (I2C0_S & I2C_S_BUSY) {
        uprintf("I2C bus is busy (S: %b, C1: %b)\n", I2C0_S, I2C0_C1);

        /* A STOP condition was missed for some reason.  Turn off the
         * IIC module and try to create it manually. several of these
         * in succession might conseivably also count as clocking the
         * slave, which might help if it is stuck. */

        I2C0_C1 = 0;

        /* SCL, SDA high */

        GPIOB_PDDR |= (PT(2) | PT(3));
        GPIOB_PSOR = (PT(2) | PT(3));
        PORTB_PCR2 = PORTB_PCR3 = (
            PORT_PCR_MUX(1) | PORT_PCR_ODE | PORT_PCR_DSE);

        /* SCL low */

        GPIOB_PCOR = PT(2);
        delay_us(1);

        /* SDA low */

        GPIOB_PCOR = PT(3);
        delay_us(9);

        /* SCL high */

        GPIOB_PSOR = PT(2);
        delay_us(1);

        /* SDA high, i.e. STOP */

        GPIOB_PSOR = PT(3);
    } else if (run[0]) {
        read_noblock(NSA2862X, 0x2, 1);
    } else if (run[1]) {
        read_noblock(NAU7802, 0x0, 1);
    } else {
        return;
    }

    PDB0_SC |= PDB_SC_SWTRIG;
}

__attribute__((interrupt ("IRQ"))) void i2c0_isr(void)
{
    I2C0_S |= I2C_S_IICIF;

    if (I2C0_S & I2C_S_ARBL) {
        uprintf("I2C master lost arbitration.\n");
        goto error;
    }

    WAIT_WHILE(!(I2C0_S & I2C_S_TCF), 10);

    if (context.phase <= 2 && (I2C0_S & I2C_S_RXAK)) {
        uprintf("No ACK from I2C slave.\n");
        goto error;
    }

    switch (context.phase) {
    case 0:
        /* Slave selected; select the regsiter. */

        I2C0_D = context.reg;

        context.phase++;
        return;

    case 1:
        if (context.buffer) {
            /* Switch to RX and select the register for reading. */

            I2C0_C1 |= I2C_C1_RSTA;
            I2C0_D = context.slave | 1;
        } else {
            /* Write the register. */

            I2C0_D = context.value;
        }

        context.phase++;
        return;

    case 2:
        if (context.buffer) {
            if (context.length == 1) {
                I2C0_C1 |= I2C_C1_TXAK;
            } else {
                I2C0_C1 &= ~I2C_C1_TXAK;
            }

            I2C0_C1 &= ~I2C_C1_TX;
            I2C0_D;

            context.phase++;
        } else {
            I2C0_C1 &= ~I2C_C1_MST;

            /* This is a write.  Currently we don't need any writes,
             * so we shouldn't end up here. */

            uassert(false);
        }
        return;

    default:
        const int i = context.phase - 3;

        if (i == context.length - 2) {
            I2C0_C1 |= I2C_C1_TXAK;
        } else if (i == context.length - 1) {
            /* From the reference manual:

             * NOTE: When making the transition out of master receive
             * mode, switch the I2C mode before reading the Data register
             * to prevent an inadvertent initiation of a master receive
             * data transfer.
             */

            I2C0_C1 |= I2C_C1_TX;
        }

        context.buffer[i] = I2C0_D;

        if (i == context.length - 1) {
            I2C0_C1 &= ~I2C_C1_MST;
            WAIT_WHILE(I2C0_S & I2C_S_BUSY, 10);

            if (context.length == 1) {
                /* This is the read-back of the register containing
                 * the data-ready bit.  If the conversion is not done
                 * yet move on to the next slave, otherwise set up a
                 * read of the value registers. */

                switch (context.slave) {
                case NSA2862X:
                    if (context.buffer[0] & 0x1) {
                        read_noblock(NSA2862X, 0x6, 3);
                    } else if (run[1]) {
                        read_noblock(NAU7802, 0x0, 1);
                    }

                    break;
                case NAU7802:
                    if (context.buffer[0] & 0x20) {
                        read_noblock(NAU7802, 0x12, 3);
                    }

                    break;
                }
            } else if (context.length == 3) {
                const int32_t c =
                    (context.buffer[0] << 16)
                    | (context.buffer[1] << 8)
                    | context.buffer[2];

                const int32_t d = c & (1 << 23) ? c - (1 << 24) : c;

                switch (context.slave) {
                case NSA2862X:
                {
                    /* Calculate the pressure in bars. */

                    const double P = (double)12.0 * ldexp(d, -23);
                    filter_sample(&pressure_filter, P, get_time());

                    RUN_CALLBACKS(
                        pressure_callbacks,
                        bool (*)(double, double, double, double, double, int32_t),
                        pressure_filter.y, pressure_filter.dy,
                        pressure_filter.t, pressure_filter.dt,
                        P, d);

                    if (run[1]) {
                        read_noblock(NAU7802, 0x0, 1);
                    }

                    break;
                }
                case NAU7802:
                {
                    const double t = get_time();

                    static double tare, m[2], s[2];
                    static int n[2];

                    /* Calculate the mass in grams. */

                    const double y =
                        (double)1.3287e-03 * d - (double)5.6135e+02;

                    /* There's a lot of noise in the load cell's
                     * signal.  Handling it via exponential smoothing
                     * leads to long filter delays, esp. for the
                     * derivative.  We therefore average 20, or about
                     * 100ms worth os samples and then perform
                     * exponential smoothing on these averaged values.
                     * This gives a usable derivative signal, albeit
                     * at a reduced output rate of 10Hz. */

                    n[0]++;
                    const double m_0 = m[0];
                    m[0] += (y - m_0) / n[0];
                    s[0] += (y - m_0) * (y - m[0]);

                    if (n[0] < 20) {
                        break;
                    }

                    s[0] /= (n[0] - 1);

                    /* Mass accuracy is important when the reading is
                     * stable.  When weighing coffee beans for
                     * instance, a tolerance below 0.1g might be
                     * significant, when the weight increases at 2-3
                     * g/s as coffee is produced, a 0.1g tolerance is
                     * arguably of less importance.  We therefore keep
                     * accumulating averaged values when successive
                     * means are close enough, giving increased noise
                     * reduction the longer you wait. */

                    const bool p = fabs(m[0] - m[1]) < (double)0.25;

                    if (p) {
                        const double nn = n[0] + n[1];
                        const double mm = m[0] - m[1];

                        s[1] =
                            ((n[0] - 1) * s[0] + (n[1] - 1) * s[1]) / (nn - 1)
                            + n[0] * n[1] * mm * mm / nn / (nn - 1);
                        m[1] = (m[0] * n[0] + m[1] * n[1]) / (n[0] + n[1]);
                        n[1] += n[0];
                    }

                    /* Take a first tare at 3s then stay in taring
                     * mode, while no disturbance is detected, but
                     * don't accumulate more than 5s worth os samples
                     * at any time, as there seems to be drift in the
                     * sensor's ouput (perhaps due to
                     * temperature?). */

                    if ((taring_state == 1 && n[1] == 600)
                        || (taring_state > 1 && n[1] == 1000)) {
                        tare = m[1];
                        mass_filter.y = mass_filter.dy = 0;
                        taring_state++;
                    }

                    if (!p || n[1] > 1000) {
                        s[1] = s[0];
                        m[1] = m[0];
                        n[1] = n[0];
                    }

                    filter_sample(&mass_filter, m[1] - tare, t);

                    if (fabs(mass_filter.dy / mass_filter.dt) > 100.0) {
                        taring_state = 1;
                    } else if (!p && taring_state > 1) {
                        taring_state = 0;
                    }

                    RUN_CALLBACKS(
                        mass_callbacks,
                        bool (*)(double, double, double, double, double, int32_t),
                        mass_filter.y, mass_filter.dy,
                        mass_filter.t, mass_filter.dt,
                        m[0], d);

                    s[0] = m[0] = n[0] = 0;

                    break;
                }
                }
            } else {
                uassert(false);
            }
        } else {
            context.phase++;
        }

        return;
    }

    uassert(false);

error:
    /* There was an error, stop the current transmission and restart a
     * conversion if required. */

    uprintf("I2C error in IRQ (S: %b, C1: %b)\n", I2C0_S, I2C0_C1);

    I2C0_S |= I2C_S_ARBL;
    I2C0_C1 = I2C_C1_IICEN;
}

bool probe_i2c(uint8_t slave)
{
    bool p = false;

    WAIT_WHILE(I2C0_S & I2C_S_BUSY, 100);

    I2C0_C1 &= ~I2C_C1_IICIE;
    I2C0_S |= I2C_S_IICIF;

    I2C0_C1 |= (I2C_C1_MST | I2C_C1_TX);
    WRITE(slave | 0);

    p = true;

error:
    I2C0_C1 &= ~I2C_C1_MST;

    return p;
}

uint8_t read_i2c(uint8_t slave, uint8_t reg, uint8_t *buffer, size_t n)
{
    size_t i;

    WAIT_WHILE(I2C0_S & I2C_S_BUSY, 100);

    I2C0_C1 &= ~I2C_C1_IICIE;
    I2C0_S |= I2C_S_IICIF;

    /* Start the transmission. */

    I2C0_C1 |= (I2C_C1_MST | I2C_C1_TX);

    /* Select the slave and register to read. */

    WRITE(slave | 0);
    WRITE(reg);

    /* Do a repeated start to switch to reception. */

    I2C0_C1 |= I2C_C1_RSTA;
    WRITE(slave | 1);

    /* Switch to reception and prime the data register. */

    if (n == 1) {
        I2C0_C1 |= I2C_C1_TXAK;
    } else {
        I2C0_C1 &= ~I2C_C1_TXAK;
    }

    I2C0_C1 &= ~I2C_C1_TX;
    READ();

    /* Read all but the last bytes. */

    for (i = 0 ; i < n - 1 ; i += 1) {
        if (i == n - 2) {
            I2C0_C1 |= I2C_C1_TXAK;
        }

        if (buffer) {
            buffer[i] = READ();
        }

        WAIT_WHILE(!(I2C0_S & I2C_S_TCF), 50);
    }

    /* From the reference manual:

     * NOTE: When making the transition out of master receive
     * mode, switch the I2C mode before reading the Data register
     * to prevent an inadvertent initiation of a master receive
     * data transfer.
     */

    I2C0_C1 |= I2C_C1_TX;

    if (buffer) {
        buffer[i] = I2C0_D;
    }

error:
    I2C0_C1 &= ~I2C_C1_MST;

    return I2C0_D;
}

void write_i2c(uint8_t slave, uint8_t reg, uint8_t value)
{
    WAIT_WHILE(I2C0_S & I2C_S_BUSY, 100);

    I2C0_C1 &= ~I2C_C1_IICIE;
    I2C0_S |= I2C_S_IICIF;

    /* Start the transmission. */

    I2C0_C1 |= (I2C_C1_MST | I2C_C1_TX);

    /* Select the slave, register and write the value. */

    WRITE(slave | 0);
    WRITE(reg);
    WRITE(value);

    /* Stop the transaction. */

error:
    I2C0_C1 &= ~I2C_C1_MST;
}

#undef READ
#undef WRITE
#undef WAIT

void reset_i2c(void)
{
    SIM_SCGC4 |= SIM_SCGC4_I2C0;
    SIM_SCGC5 |= SIM_SCGC5_PORTC | SIM_SCGC5_PORTB;

    /* Sensor power */

    PORTC_PCR2 = PORT_PCR_MUX(1) | PORT_PCR_DSE;
    GPIOC_PDDR |= PT(2);
    GPIOC_PSOR = PT(2);

    PORTB_PCR2 = PORTB_PCR3 = (PORT_PCR_MUX(1) | PORT_PCR_PE);

    delay_ms(100);

    /* Read SCL, SDA to detect presence of pull-ups.  Return if bus is
     * not attached. */

    if ((GPIOB_PDIR & (PT(2) | PT(3))) != (PT(2) | PT(3))) {
        return;
    }

    /* SCL, SDA */

    PORTB_PCR2 = PORTB_PCR3 = (PORT_PCR_MUX(2) | PORT_PCR_ODE | PORT_PCR_DSE);

    /* Enable the I2C module and set the bus speed to 400Khz with a
     * SDA hold time of 0.75us, a SCL start hold time of 0.92us and a
     * SCL stop hold time of 1.33us. */

    I2C0_F = I2C_F_MULT(2) | I2C_F_ICR(5);
    I2C0_FLT = I2C_FLT_FLT(31);
    I2C0_C1 |= I2C_C1_IICEN;
    I2C0_C2 |= I2C_C2_HDRS;

    run[0] = probe_i2c(NSA2862X);
    run[1] = probe_i2c(NAU7802);

    if (!(run[0] || run[1])) {
        SIM_SCGC4 &= ~SIM_SCGC4_I2C0;

        return;
    }

    prioritize_interrupt(I2C0_IRQ, 8);
    enable_interrupt(I2C0_IRQ);

    if (run[1]) {
        /* Reset the NAU7802. */

        write_i2c(NAU7802, 0x0, 0x1);  /* Reset */
        write_i2c(NAU7802, 0x0, 0x2);  /* Power up digital */

        delay_us(300);
        if (!(read_i2c(NAU7802, 0x0, NULL, 1) & 0x8)) {
            run[1] = false;
        }
    }

    if (run[1]) {
        write_i2c(NAU7802, 0x11, 0x30); /* Switch to strong pull-up. */
        write_i2c(NAU7802, 0x1, 0x2f); /* 3.0V, x128 */
        write_i2c(NAU7802, 0x15, 0x30); /* Disable the ADC chopper. */
        write_i2c(NAU7802, 0x1c, 0x80); /* Enable the PGA capacitor. */
        write_i2c(NAU7802, 0x2, 0x74); /* 320Hz, start calibration */

        uint8_t u;
        WAIT_WHILE(((u = read_i2c(NAU7802, 0x2, NULL, 1)) & 0x4), 100000);

        if (u & 0x8) {
          error:
            run[1] = false;
        } else {
            write_i2c(NAU7802, 0x0, 0x96);
            delay_ms(600);
        }
    }

    /* Enable the PDB module, to use as a simple interrupt timer. */

    SIM_SCGC6 |= SIM_SCGC6_PDB;

    /* The PDB is clocked by the bus clock so the delay is
     *   IDLY * MULT / 48e6

     * seconds.  The NSA2862X converts at 150Hz, while the NAU7802
     * converts at 320Hz.  Set the timer to 5ms, which seems to be a
     * good compromise. */

    PDB0_IDLY = 24000;
    PDB0_SC = (PDB_SC_MULT(1) | PDB_SC_TRGSEL(15) | PDB_SC_PDBEN | PDB_SC_PDBIE
               | PDB_SC_LDOK);

    prioritize_interrupt(PDB0_IRQ, 8);
    enable_interrupt(PDB0_IRQ);

    PDB0_SC |= PDB_SC_SWTRIG;
}

#undef WAIT_WHILE
