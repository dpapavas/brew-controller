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

#ifndef PID_H
#define PID_H

#include <stdbool.h>

struct pid {
    double K_p;
    double T_i;
    double T_d;

    double set;
    double integral;
};

double calculate_pid_output(struct pid *pid, double y, double dy, double dt);
void reset_pid(struct pid *pid, double set);

#endif
