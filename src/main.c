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

#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "callbacks.h"
#include "fonts.h"
#include "i2c.h"
#include "mk20dx.h"
#include "peripherals.h"
#include "profile.h"
#include "time.h"
#include "uassert.h"
#include "usb.h"

#define K_U 0.125
#define P_U 16.5

struct pid temperature_pid = {0.6 * K_U, 0.5 * P_U, 0.125 * P_U, /* 104.0 */NAN};

#undef K_U
#undef P_U

#define K_U 0.5
#define P_U 4

struct pid pressure_pid = {0.6 * K_U, 0.5 * P_U, 0.125 * P_U, NAN};

#undef K_U
#undef P_U

#define K_U 0.24
#define P_U 0.6

struct pid flow_pid = {K_U / 3, 0.5 * P_U, P_U / 3, NAN};

#undef K_U
#undef P_U

double strtod(const char *s, char **e)
{
    int n = 0, i = 1, u = 0;
    bool p = false;

    if (!strncmp(s, "nan", 3)) {
        if (e) {
            *e = (char *)s + 3;
        }

        return NAN;
    }

    if (*s == '-') {
        i = -1;
        s++;
    } else if (*s == '+') {
        s++;
    }

    if (!strncmp(s, "inf", 3)) {
        if (e) {
            *e = (char *)s + 3;
        }

        return i * INFINITY;
    }

    while (*s) {
        if (isdigit((int)*s)) {
            u = u * 10 + *s++ - '0';

            if (p) {
                n--;
            }
        } else if (!p && *s == '.') {
            p = true;
            s++;
        } else {
            break;
        }
    }

    if (e) {
        *e = (char *)s;
    }

    return (i * u * exp10(n));
}

static int log_line_count;

#define LOGGING_CALLBACK_BODY(FMT, ...)         \
    {                                           \
        if (log_line_count == 0) {              \
            return true;                        \
        }                                       \
                                                \
        if (log_line_count > 0) {               \
            log_line_count--;                   \
        }                                       \
                                                \
        uprintf(FMT, __VA_ARGS__);              \
                                                \
        return false;                           \
    }

static bool turn_logging_callback(int delta)
LOGGING_CALLBACK_BODY("wheel %d\n", delta)

static bool click_logging_callback(bool down)
LOGGING_CALLBACK_BODY("wheel %s\n", down ? "down" : "up")

static bool panel_logging_callback(bool down)
LOGGING_CALLBACK_BODY("panel %s\n", down ? "down" : "up")

static bool shot_logging_callback(void)
LOGGING_CALLBACK_BODY(
    "%.3f, %.3f, %.3f, %.3f, %.3f, %.3f, %.3f, %.3f\n",
    (double)get_time(),
    (double)temperature_filter.y,
    (double)pressure_filter.y,
    (double)get_flow(),
    (double)get_volume(),
    (double)mass_filter.y,
    (double)get_heat_power(),
    (double)get_pump_flow());

#define DEFINE_SENSOR_LOGGING_CALLBACK(WHAT)                            \
static bool WHAT ##_logging_callback(                                   \
    double y, double dy, double t, double dt,  double y_raw, int32_t c) \
LOGGING_CALLBACK_BODY(                                                  \
    "%.3f, %.3f, %.3f, %.3f, %d\n",                                     \
    (double)t, (double)y, (double)(dy / dt), (double)y_raw, c)

DEFINE_SENSOR_LOGGING_CALLBACK(temperature)
DEFINE_SENSOR_LOGGING_CALLBACK(pressure)
DEFINE_SENSOR_LOGGING_CALLBACK(mass)
DEFINE_SENSOR_LOGGING_CALLBACK(flow)

#define DEFINE_PID_LOGGING_CALLBACK(WHAT, X)                            \
static bool WHAT ##_pid_logging_callback(                               \
    double y, double dy, double t, double dt,  double y_raw, int32_t c) \
LOGGING_CALLBACK_BODY(                                                  \
    "%.3f, %.1f, %.3f, %.1f, %.3f, %.3f, %.3f\n",                       \
    (double)t, 100 * (double)X, (double)y, (double)WHAT ##_pid.set,     \
    (double)(WHAT ##_pid.K_p * (WHAT ##_pid.set - y)),                  \
    (double)(WHAT ##_pid.K_p *                                          \
             WHAT ##_pid.integral / WHAT ##_pid.T_i),                   \
    (double)(-WHAT ##_pid.K_p * WHAT ##_pid.T_d * dy / dt))

DEFINE_PID_LOGGING_CALLBACK(temperature, get_heat_power())
DEFINE_PID_LOGGING_CALLBACK(pressure, get_pump_flow())
DEFINE_PID_LOGGING_CALLBACK(flow, get_pump_flow())

#undef DEFINE_SENSOR_LOGGING_CALLBACK
#undef DEFINE_PID_LOGGING_CALLBACK
#undef LOGGING_CALLBACK_BODY

static const struct pid *pid_print_target;
static bool pid_print_callback(void)
{
    uprintf(
        "%f, %f, %f, %f, %f\n",
        (double)pid_print_target->set,
        (double)pid_print_target->K_p,
        (double)pid_print_target->T_i,
        (double)pid_print_target->T_d,
        (double)pid_print_target->integral);

    return true;
}

static uint8_t i2c_print_target;
static bool i2c_print_callback(void)
{
    uprintf("%d,%x,%b\n", i2c_print_target, i2c_print_target, i2c_print_target);

    return true;
}

static bool profile_print_callback(void)
{
    print_profile();

    return true;
}

static bool profile_stored_callback(void)
{
    const struct profile *profile = get_profile();
    uprintf("%u\n", profile->alloc);

    return true;
}

static void usb_data_in(uint8_t *data, size_t n)
{
    uassert(n > 0);

    const char *c = (char *)data;

    /* Commands:
       b: reboot into programming mode.
       l[t][N]: toggle logging of [N] lines of [temperature] data.
       hN:  set heater command to N (0-100).
       rN:  do relay (bang-bang) control with set-point N (deg. Celsius). */

    switch(*(c++)) {
    case 'b':
         __asm__ volatile ("bkpt #251");
        break;

    case 'l':
        if (log_line_count == 0) {
            unsigned long i;
            char *e;

            switch (*(c++)) {
            case 't':
                add_callback(
                    temperature_logging_callback, temperature_callbacks);
                break;
            case 'p':
                add_callback(pressure_logging_callback, pressure_callbacks);
                break;
            case 'm':
                add_callback(mass_logging_callback, mass_callbacks);
                break;
            case 'f':
                add_callback(flow_logging_callback, flow_callbacks);
                break;
            case 'T':
                add_callback(
                    temperature_pid_logging_callback, temperature_callbacks);
                break;
            case 'P':
                add_callback(pressure_pid_logging_callback, pressure_callbacks);
                break;
            case 'F':
                add_callback(flow_pid_logging_callback, flow_callbacks);
                break;
            case 's':
                add_callback(shot_logging_callback, tick_callbacks);
                break;
            case 'c':
                add_callback(panel_logging_callback, panel_callbacks);
                add_callback(click_logging_callback, click_callbacks);
                add_callback(turn_logging_callback, turn_callbacks);
                break;
            default: goto skip;
            }

            i = strtoul(c, &e, 10);
            if (e == c) {
                log_line_count = -1;
            } else {
                log_line_count = i;
            }
          skip:
        } else {
            log_line_count = 0;
        }

        break;

    case 's':
        switch(*(c++)) {
        case 'h':
        {
            switch(*(c++)) {
            case 'p':
                set_heat_power(strtod(c, NULL) / 100.0);
                break;
            case 'd':
                set_heat_delay(1.0 - strtod(c, NULL) / 100.0);
                break;
            }

            break;
        }

        case 'p':
        {
            switch(*(c++)) {
            case 'f':
                set_pump_flow(strtod(c, NULL) / 100.0);
                break;
            case 'p':
                set_pump_power(strtod(c, NULL) / 100.0);
                break;
            case 'd':
                set_pump_delay(1.0 - strtod(c, NULL) / 100.0);
                break;
            }

            break;
        }
        }

        break;

    case 'c':
        struct pid *pid;

        switch(*(c++)) {

        case 'h':
            pid = &temperature_pid;
            goto set;
        case 'p':
            pid = &pressure_pid;
            goto set;
        case 'f':
            pid = &flow_pid;
            goto set;
        set:
            {
                double f;
                char *e;

                double * const p[] = {
                    &pid->set,
                    &pid->K_p,
                    &pid->T_i,
                    &pid->T_d};

                for (size_t i = 0; i < sizeof(p) / sizeof(p[0]); i++) {
                    f = strtod(c, &e);

                    if (e == c) {
                        break;
                    }

                    *p[i] = f;

                    if (*e != ',') {
                        break;
                    }

                    c = ++e;
                }

                pid->integral = 0;

                pid_print_target = pid;
                add_callback(pid_print_callback, tick_callbacks);

                break;
            }

        case 'i':
            {
                char *e;
                uint8_t a[3];
                int i;

                for (i = 0; i < 3; i++) {
                    a[i] = strtol(c, &e, 16);

                    if (e == c || (*e != ',' && *e != '\0')) {
                        break;
                    }

                    c = ++e;
                }

                if (i == 3) {
                    write_i2c(a[0], a[1], a[2]);
                } else if (i == 2) {
                    read_i2c(a[0], a[1], &i2c_print_target, 1);
                    add_callback(i2c_print_callback, tick_callbacks);
                } else if (i == 1) {
                    probe_i2c(a[0]);
                }

                break;
            }
        }

        break;

    case 'z':
        switch(*(c++)) {

        case 'f':
            tare_flow();
            break;

        case 'm':
            tare_mass();
            break;
        }

        break;

    case 't':
        switch(*(c++)) {

        case 's':
            /* Toggle sensor power. */

            GPIOC_PTOR = PT(2);
            break;

        case 't':
        {
            unsigned long i;
            char *e;

            i = strtoul(c, &e, 10);
            if (e > c) {
                run_temperature((bool)i);
            } else {
                read_temperature();
            }

            break;
        }

        case 'p':
        {
            unsigned long i;
            char *e;

            i = strtoul(c, &e, 10);
            if (e > c) {
                run_pressure((bool)i);
            } else {
                read_pressure();
            }

            break;
        }

        case 'm':
        {
            unsigned long i;
            char *e;

            i = strtoul(c, &e, 10);
            if (e > c) {
                run_mass((bool)i);
            } else {
                read_mass();
            }

            break;
        }
        }

        break;

    case 'p':
        switch(*(c++)) {

        case 's':
            if (read_profile(c)) {
                add_callback(profile_stored_callback, tick_callbacks);
            }
            break;

        case 'p':
            add_callback(profile_print_callback, tick_callbacks);
            break;
        }

        break;

    case 'r':
        /* Request a system reset. */

        SCB_AIRCR = 0x5fa0004;
        break;
    }
}

static enum {
    AUTO,
    MANUAL_TEMPERATURE,
    MANUAL_FLOW,
    MANUAL_PRESSURE,
    MANUAL_HEAT,
    MANUAL_PUMP,

    MODES
} mode;

static bool temperature_pid_callback(
    double T, double dT, double t, double dt, double T_raw, uint16_t c)
{
    if (isnan(temperature_pid.set) || isnan(dt)) {
        return false;
    }

    uassert(dt > 0);

    set_heat_power(
        isnan(T) || isnan(dT)
        ? 0
        : calculate_pid_output(&temperature_pid, T, dT, dt));

    return false;
}

static bool pressure_pid_callback(
    double P, double dP, double t, double dt, double P_raw, uint16_t c)
{
    if (isnan(pressure_pid.set) || isnan(dt)) {
        return false;
    }

    uassert(dt > 0);

    set_pump_flow(
        isnan(P) || isnan(dP)
        ? 0
        : fmax(0.01, calculate_pid_output(&pressure_pid, P, dP, dt)));

    return false;
}

static bool flow_pid_callback(
    double Q, double dQ, double t, double dt,  double V, double raw, uint32_t n)
{
    if (isnan(flow_pid.set) || isnan(dt)) {
        return false;
    }

    uassert(dt > 0);

    set_pump_flow(
        isnan(Q) || isnan(dQ)
        ? 0
        : fmax(0.01, calculate_pid_output(&flow_pid, Q, dQ, dt)));

    return false;
}

static bool mode_switch_callback(bool down)
{
    if (down) {
        return false;
    }

    do {
        mode = (mode + 1) % MODES;
    } while (!isnan(get_shot_time())
             && mode != AUTO
             && mode != MANUAL_FLOW
             && mode != MANUAL_PRESSURE
             && mode != MANUAL_PUMP);

    enable_profile(mode == AUTO);

    return false;
}

static bool adjust_callback(int delta)
{
#define ADJUST(X, DX, MIN, MAX) fmin(MAX, fmax(MIN, X + delta * DX))
    switch (mode) {
    case AUTO:
        break;

    case MANUAL_TEMPERATURE:
        temperature_pid.set =
            isnan(temperature_pid.set)
            ? 100
            : ADJUST(temperature_pid.set, 1, 20, 120);
        break;

    case MANUAL_PRESSURE:
        flow_pid.set = NAN;
        pressure_pid.set =
            isnan(pressure_pid.set)
            ? fmax(0, pressure_filter.y)
            : ADJUST(pressure_pid.set, 0.1, 0, 10);
        break;

    case MANUAL_FLOW:
        pressure_pid.set = NAN;
        flow_pid.set =
            isnan(flow_pid.set)
            ? flow_filter.y
            : ADJUST(flow_pid.set, 0.1, 0, 10);
        break;

    case MANUAL_HEAT:
        temperature_pid.set = NAN;
        set_heat_power(ADJUST(get_heat_power(), 0.05, 0, 1));
        break;

    case MANUAL_PUMP:
        flow_pid.set = NAN;
        pressure_pid.set = NAN;
        set_pump_flow(ADJUST(get_pump_flow(), 0.05, 0, 1));
        break;

    case MODES:
        uassert(false);
    }
#undef ADJUST

    return false;
}

int main(void)
{
    /* Configure interrupt priority grouping.  We select one group
     * bit, meaning that priorities >= 8 belong to the low-priority
     * group and can be preempted by IRQs with priority < 8. */

    SCB_AIRCR = 0x5fa0600;
    enable_interrupts();

    /* SysTick timer */

#ifdef TEENSY30
    SYST_RVR = 4800000 - 1;
#else
    SYST_RVR = 7200000 - 1;
#endif
    SYST_CVR = 0;
    SYST_CSR = SYST_CSR_CLKSOURCE | SYST_CSR_TICKINT | SYST_CSR_ENABLE;

    /* PIT */

    SIM_SCGC6 |= SIM_SCGC6_PIT;
    PIT_MCR &= ~PIT_MCR_MDIS;

    /* SPI */

    SIM_SCGC5 |= SIM_SCGC5_PORTD | SIM_SCGC5_PORTC;
    SIM_SCGC6 |= SIM_SCGC6_SPI0;

    PORTC_PCR6 = PORT_PCR_MUX(2) | PORT_PCR_DSE; /* DOUT */
    PORTC_PCR7 = PORT_PCR_MUX(2) | PORT_PCR_DSE; /* DIN */
    PORTD_PCR1 = PORT_PCR_MUX(2) | PORT_PCR_DSE; /* SCK */

    SPI0_MCR &= ~SPI_MCR_MDIS;
    SPI0_MCR = SPI_MCR_MSTR;

    reset_time();
    reset_usb();
    set_usb_data_in_callback(usb_data_in);

#if 0
    await_usb_dtr();
#endif

    /* Initialize the peripherals. */

    reset_input();
    reset_power();
    reset_temperature();
    reset_i2c();
    reset_flow();
    reset_display();
    reset_profile();
    enable_profile(mode == AUTO);

    add_callback(temperature_pid_callback, temperature_callbacks);
    add_callback(pressure_pid_callback, pressure_callbacks);
    add_callback(flow_pid_callback, flow_callbacks);
    add_callback(mode_switch_callback, click_callbacks);
    add_callback(adjust_callback, turn_callbacks);

    clear_display();

    display("\a\4\x0a\1Time:\a\x44\x0a\1Temp:");
    display("\a\4\x2a\1Flow:\a\x44\x2a\1Pres:");
    display("\a\4\x4a\1Vol:\a\x44\x4a\1Mass:");
    display("\a\4\x6a\1Heat:\a\x44\x6a\1Pump:");

    display("\x3f\a\4\x1d\2-  \v");

    while (1) {
        static bool p_0;
        const bool p = fmod(get_time(), 0.5) < 0.25;

#define UPDATE_DISPLAY(X, BLINK, POS, VALUE, UNIT)                      \
        static double X ##_0 = -INFINITY;                               \
        const double X = VALUE;                                         \
                                                                        \
        if (((BLINK) && p != p_0)                                       \
            || (!(isnan(X ##_0) && isnan(X)) && X ##_0 != X)) {         \
            if (BLINK && p) {                                           \
                display("\a" POS "\2\v");                               \
                X ##_0 = -INFINITY;                                     \
            } else {                                                    \
                if (isnan(X)) {                                         \
                    display("\a" POS "\2-  \v");                        \
                } else {                                                \
                    if (fabs(X) < 1000) {                               \
                        display("\a" POS "\2%.1f\1" UNIT "\v", (double)X); \
                    } else {                                            \
                        display("\a" POS "\2%.0f\1" UNIT "\v", (double)X); \
                    }                                                   \
                }                                                       \
                                                                        \
                X ##_0 = X;                                             \
            }                                                           \
        }

        /* Shot time */

        UPDATE_DISPLAY(t, false, "\4\x1d", get_shot_time(), "s");

        /* Temperature */

        UPDATE_DISPLAY(
            T, mode == MANUAL_TEMPERATURE, "\x44\x1d",
            (mode == MANUAL_TEMPERATURE
             ? temperature_pid.set
             : (temperature_filter.y < 0 ? 0 : temperature_filter.y)),
            "\2\x7f");

        /* Flow */

        {
            const double x = (mode == MANUAL_FLOW ? flow_pid.set : get_flow());

            UPDATE_DISPLAY(
                Q, mode == MANUAL_FLOW, "\4\x3d",
                (x >= 10 ? 9.9 : x),
                "ml/s");
        }

        /* Pressure */

        UPDATE_DISPLAY(
            S, mode == MANUAL_PRESSURE, "\x44\x3d",
            (mode == MANUAL_PRESSURE
             ? pressure_pid.set
             : (pressure_filter.y < 0 ? 0 : pressure_filter.y)),
            "bar");

        /* Mass */

        UPDATE_DISPLAY(
            M, is_taring_mass(), "\x44\x5d", mass_filter.y, "g");

        /* Volume */

        UPDATE_DISPLAY(V, false, "\4\x5d", get_volume(), "ml");

        /* Heating power */

        UPDATE_DISPLAY(
            H, mode == MANUAL_HEAT, "\4\x7d",
            get_heat_power() * 100, "%%");

        /* Pump power */

        UPDATE_DISPLAY(
            W, mode == MANUAL_PUMP,"\x44\x7d",
            get_pump_flow() * 100, "%%");

        p_0 = p;

#undef UPDATE_DISPLAY
    }
}
