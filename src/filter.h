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

#ifndef FILTER_H
#define FILTER_H

struct filter {
    double tau, sigma;
    double y, dy, t, dt;
};

void filter_sample_dt(struct filter *f, double y_k, double dt);
void filter_sample(struct filter *f, double y_k, double t);

#define SINGLE_FILTER(TAU) (struct filter){TAU, NAN, NAN, NAN, NAN, NAN}
#define DOUBLE_FILTER(TAU, SIGMA) \
    (struct filter){TAU, SIGMA, NAN, NAN, NAN, NAN}
#endif
