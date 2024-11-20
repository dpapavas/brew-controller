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

#ifndef PROFILE_H
#define PROFILE_H

#include <stdbool.h>
#include <stddef.h>

struct profile {
    size_t size, alloc;

    struct stage {
        enum {
            FLOW_INPUT,
            PRESSURE_INPUT,
            TIME_INPUT,
            VOLUME_INPUT,
            MASS_INPUT,
        } input;

        enum {
            FLOW_OUTPUT,
            POWER_OUTPUT,
            PRESSURE_OUTPUT,
        } output;

        enum mode {
            ABSOLUTE,
            RATIOMETRIC,
            RELATIVE,
        } input_mode, output_mode;

        bool ease_input, ease_output;
        double (*points)[4];

        enum action {
            BACK,
            RESET_VOLUME,
            RESET_TIME,
            RESET_MASS,
        } *actions;

        size_t sizes[2];
    } *stages;
};

void reset_profile(void);
void enable_profile(bool enable);
double get_shot_time(void);
size_t get_stage(void);
size_t get_stages(void);

const struct profile *get_profile(void);
void print_profile(void);
bool read_profile(const char *s);

#endif
