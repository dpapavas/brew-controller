#ifndef PROFILE_H
#define PROFILE_H

#include <stdbool.h>
#include <stddef.h>

struct profile {
    size_t size, alloc;

    struct stage {
        enum {
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

        bool ease_in;
        double (*points)[4];

        enum action {
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
