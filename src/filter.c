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
#include "filter.h"

void filter_sample_dt(struct filter *f, double y_k, double dt)
{
    if (isnan(f->y)) {
        f->y = y_k;
    } else if (isnan(f->dy)) {
        f->dy = y_k - f->y;
        f->y = y_k;
    } else {
        const double a = exp(-dt / f->tau);

        if (!isnan(f->sigma)) {
            const double g = exp(-dt / f->sigma);

            f->y = a * (f->y + f->dy) + (1 - a) * y_k;
            f->dy = g * f->dy + (1 - g) * (y_k - f->y);
        } else {
            f->dy = (a - 1) * f->y + (1 - a) * y_k;
            f->y += f->dy;
        }
    }

    f->dt = dt;
}

void filter_sample(struct filter *f, double y_k, double t)
{
    double dt = t - f->t;

    filter_sample_dt(f, y_k, dt);
    f->t = t;
}

#ifdef TEST
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int main(int argc, char *argv[])
{
    struct filter filter = DOUBLE_FILTER(1, 0.1);

    bool d = false;
    int opt, average = 0;

    while ((opt = getopt(argc, argv, "da:")) != -1) {
        switch (opt) {
        case 'd':
            d = true;
            break;
        case 'a':
            average = atoi(optarg);
            break;
        default:
            return 1;
        }
    }

    /* printf("$data << end\n"); */

    double I = 0, P = 0, y = 0, y_0 = NAN;
    int n, m;

    for (n = 0; ; n++) {
        double t_k, y_k, dy_dt_k, y_f;
        unsigned int c;

        if (scanf(
                "%f,%f,%f,%f,%u\n",
                &t_k, &y_f, &dy_dt_k, &y_k, &c) == EOF) {
            break;
        }

#if 0
        if (t_k < 300) {
            y_k = 100;
        } else if (t_k < 310) {
            y_k = 100 - 20 * sin((t_k - 300) / 20 * M_PI);
        } else {
            y_k = 80;
        }
#endif

        if (average > 0) {
            y += y_k;
            m++;

            if (m < average) {
                continue;
            }

            y /= m;
        } else {
            y = y_k;
        }

        filter_sample(&filter, y, t_k);

        if (!d) {
            printf("%f, %f, %f\n", t_k, y, filter.y);
        }

        if (isnan(filter.dy)) {
            goto skip;
        }

        if (d) {
            printf(
                "%f, %f, %f\n",
                t_k, (y - y_0) / filter.dt, filter.dy / filter.dt);
        }

        I += filter.dy;
        P += ((filter.dy / filter.dt)
              * (filter.dy / filter.dt));

      skip:
        y_0 = y;
        y = m = 0;
    }

    printf("end\n");
    printf("set datafile separator ','\n");
    printf("plot $data using 1:2 with lines,  $data using 1:3 with lines\n");
    printf("set print '-'\n");
    printf("print 'Integral: %f'\n", I);
    printf("print 'RMS differential: %f'\n", sqrt(P / n));
    printf("pause mouse keypress \"Hit enter\"\n");

    return 0;
}
#endif
