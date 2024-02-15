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
#include "pid.h"

/* See  Astrom, K.J. and Rundqwist, L. (1989).
   "Integrator windup and how to avoid it".
   Proc. 1989 Am. Control Confi.: 1693-1698. */

/* #define BACK_CALCULATION */
#define CONDITIONAL_INTEGRATION

double calculate_pid_output(struct pid *pid, double y, double dy, double dt)
{
    const double e = pid->set - y;

#ifdef CONDITIONAL_INTEGRATION
    const double a = pid->set + pid->integral / pid->T_i;

    if ((y >= a - 1.0 / pid->K_p && y <= a) || e * pid->integral < 0) {
#endif
        pid->integral += e * dt;
#ifdef CONDITIONAL_INTEGRATION
    }
#endif

    const double u = pid->K_p * (
        e + pid->integral / pid->T_i - pid->T_d * dy / dt);

    if (u < 0 || u > 1) {
        const double v = (u > 0);
#ifdef BACK_CALCULATION
        pid->integral += (1 - exp(-dt / 0.1)) * (v - u) * pid->T_i / pid->K_p;
#endif
        return v;
    }

    return u;
}

#ifdef TEST
#include <stdbool.h>
#include <stdio.h>
#include <math.h>

/* #define TUNE */
#define K_U 0.28
#define T_U 100

#ifdef TUNE
struct pid temperature_pid = {K_U, INFINITY, 0};
#else

/* Bang-bang */
struct pid temperature_pid = {INFINITY, INFINITY, 0};

/* P */
/* struct pid temperature_pid = {0.5 * K_U, INFINITY, 0}; */

/* PI */
/* struct pid temperature_pid = {0.2 * K_U, 0.45 * T_U, 0}; */

/* PD */
/* struct pid temperature_pid = {0.8 * K_U, INFINITY, T_U / 8.0}; */

/* Classic PID */
/* struct pid temperature_pid = {0.6 * K_U, 0.5 * T_U, T_U / 8.0}; */

/* Pessen integral rule */
/* struct pid temperature_pid = {0.7 * K_U, 0.4 * T_U, 0.15 * T_U}; */

/* Some overshoot */
/* struct pid temperature_pid = {K_U / 3.0, 0.5 * T_U, T_U / 3.0}; */

/* No overshoot */
/* struct pid temperature_pid = {0.2 * K_U, 0.5 * T_U, T_U / 3.0}; */
#endif

int main(void)
{
    const double delay = 60;
    const double dt = 0.1;
    const int buffer_size = (int)ceil(delay / dt);

    double buffer[buffer_size];

    for (int i = 0; i < buffer_size; i++) {
        buffer[i] = 0;
    }

    const double tau = 3500;
    const double k = 450;
    const double T_a = 20;
    double T = 20;
    double t = 0;

    reset_pid(&temperature_pid, 64);

    printf("$data << end\n");

    int i = 0;
    while (t < 3500) {
        const int j = i % buffer_size;
        const double dT = (k * buffer[j] + T_a - T) / tau * dt;

        i++;
        T += dT;
        t += dt;

#ifdef TUNE
        buffer[j] = (T < 64);
#else
        buffer[j] = calculate_pid_output(&temperature_pid, T, dT, dt);
#endif

        if (i % 10) {
            printf("%f,%f,%f\n", t, T, buffer[j] * 100);
        }
    }
    printf("end\n");
    printf("set datafile separator ','\n");
    printf("plot $data using 1:2 with lines,  $data using 1:3 with lines axis x1y2\n");
}
#endif
