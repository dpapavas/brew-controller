#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "callbacks.h"
#include "mk20dx.h"
#include "peripherals.h"
#include "profile.h"
#include "time.h"
#include "usb.h"

static struct profile profile = {
    .size = 3,
    .stages = (struct stage []) {
        {.input = PRESSURE_INPUT,
         .output = FLOW_OUTPUT,
         .input_mode = ABSOLUTE,
         .output_mode = ABSOLUTE,
         .ease_in = false,
         .points = (double[][4]){{0, 3}, {4, 3}},
         .actions = (enum action []){RESET_VOLUME},
         .sizes = {2, 1}
        },

        {.input = TIME_INPUT,
         .output = PRESSURE_OUTPUT,
         .input_mode = RELATIVE,
         .output_mode = ABSOLUTE,
         .ease_in = true,
         .points = (double[][4]){{0, NAN}, {1, 0}, {30, 0}, {33, 9}, {40, 9}},
         .actions = NULL,
         .sizes = {5, 0}
        },

        {.input = VOLUME_INPUT,
         .output = FLOW_OUTPUT,
         .input_mode = ABSOLUTE,
         .output_mode = RATIOMETRIC,
         .ease_in = true,
         .points = (double[][4]){{0, NAN}, {INFINITY, 1}},
         .actions = NULL,
         .sizes = {2, 0}
        }
    }
};

/* Profile execution */

static inline double evaluate_profile_at(const double *P_i, double x)
{
    const double s = x - P_i[0];
    return P_i[1] + s * s * (P_i[2] + s * P_i[3]);
}

static void interpolate_profile_at(double (*P)[4], size_t i)
{
    const double h = P[i + 1][0] - P[i][0];
    const double d = (P[i + 1][1] - P[i][1]) / h / h;

    P[i][2] = 3 * d;
    P[i][3] = -2 * d / h;
}

static void interpolate_profile(void)
{
    for (size_t j = 0; j < profile.size; j++) {
        const struct stage *stage = &profile.stages[j];
        double (*P)[4] = stage->points;

        for (size_t i = 0; i < stage->sizes[0]; i++) {
            interpolate_profile_at(P, i);
        }
    }
}

static inline void adjust_value(
    double *x, double reference, const enum mode mode, const bool output)
{
    switch (mode) {
    case ABSOLUTE:
        break;
    case RELATIVE:
        if (output) {
            *x += reference;
        } else {
            *x -= reference;
        }
        break;
    case RATIOMETRIC:
        if (output) {
            *x *= reference;
        } else {
            *x /= reference;
        }
        break;
    }
}

static void back_calculate_pid(struct pid *pid, double dy_dt, double u)
{
    pid->integral = pid->T_i * (u + pid->K_p * pid->T_d * dy_dt) / pid->K_p;
}

static bool enabled;
static double start = NAN;
static size_t cursor;
static bool initialize_stage = true;

static bool profiling_callback(void)
{
    static double input_reference, output_reference;

    if (isnan(start) || !enabled) {
        return true;
    }

    for (; cursor < profile.size; cursor++) {
        const struct stage *stage = &profile.stages[cursor];
        double (*P)[4] = stage->points;
        double x;

        /* Establish the input. */

        switch (stage->input) {
        case TIME_INPUT:
            x = get_time() - start;
            break;

        case PRESSURE_INPUT:
            x = pressure_filter.y;
            break;

        case VOLUME_INPUT:
            x = get_volume();
            break;

        case MASS_INPUT:
            x = mass_filter.y;
            break;

        default: uassert(false);
        }

        if (initialize_stage) {
            input_reference = x;

            switch (stage->output) {
            case POWER_OUTPUT:
                output_reference = get_pump_flow();
                break;

            case PRESSURE_OUTPUT:
                pressure_pid.integral = 0;
                output_reference = pressure_filter.y;

                break;

            case FLOW_OUTPUT:
                flow_pid.integral = 0;
                output_reference = get_flow();

                break;
            }

            /* Potentially fill in a missing initial value, ensuring
             * continuity wrt the previous stage. */

            if (stage->ease_in) {
                switch (stage->output) {
                case PRESSURE_OUTPUT:
                    P[0][1] = pressure_filter.y;

                    back_calculate_pid(
                        &pressure_pid,
                        pressure_filter.dy / pressure_filter.dt,
                        get_pump_flow());

                    break;
                case FLOW_OUTPUT:
                    P[0][1] = get_flow();

                    back_calculate_pid(
                        &flow_pid, get_flow_derivative(),
                        get_pump_flow());

                    break;
                case POWER_OUTPUT:
                    P[0][1] = get_pump_flow();
                    break;
                }

                adjust_value(
                    &P[0][1], output_reference, stage->output_mode, false);
                interpolate_profile_at(P, 0);
            }

            initialize_stage = false;
        }

        adjust_value(&x, input_reference, stage->input_mode, false);

        /* Find our position in the current segment. */

        size_t i;

        for (i = 0; i < stage->sizes[0] && P[i + 1][0] <= x; i++);

        if (i == stage->sizes[0]) {
            /* We've finished the current stage. */

            for (size_t j = 0; j < stage->sizes[1]; j++) {
                switch (stage->actions[j]) {
                case RESET_VOLUME:
                    tare_flow();
                    break;
                case RESET_TIME:
                    start = get_time();
                    break;
                case RESET_MASS:
                    tare_mass();
                    break;
                }
            }

            initialize_stage = true;
            continue;
        }

        /* Evaluate the profile and set the output. */

        double y = evaluate_profile_at(P[i], x);
        adjust_value(&y, output_reference, stage->output_mode, true);

        switch (stage->output) {
        case POWER_OUTPUT:
            pressure_pid.set = NAN;
            flow_pid.set = NAN;

            set_pump_flow(y);

            break;

        case PRESSURE_OUTPUT:
            pressure_pid.set = y;
            flow_pid.set = NAN;

            break;

        case FLOW_OUTPUT:
            pressure_pid.set = NAN;
            flow_pid.set = y;

            break;
        }

        return false;
    }

    if (cursor == profile.size) {
        pressure_pid.set = NAN;
        flow_pid.set = NAN;
        set_pump_flow(0);

        return true;
    }

    return false;
}

static bool brew_callback(bool down)
{
    if (down) {
        start = get_time();

        tare_flow();

        pressure_pid.integral = 0;
        flow_pid.integral = 0;

        if (enabled) {
            initialize_stage = true;
            cursor = 0;

            add_callback(profiling_callback, tick_callbacks);
        }
    } else {
        start = NAN;
    }

    return false;
}

void reset_profile(void)
{
    interpolate_profile();
    add_callback(brew_callback, panel_callbacks);
}

void enable_profile(bool enable)
{
    if (enable && !enabled && !isnan(start)) {
        add_callback(profiling_callback, tick_callbacks);
    }

    enabled = enable;
}

size_t get_stage(void)
{
    if (enabled) {
        return cursor + 1;
    } else {
        return 0;
    }
}

size_t get_stages(void)
{
    return profile.size;
}

double get_shot_time(void)
{
    if (isnan(start)) {
        return NAN;
    }

    return get_time() - start;
}

/* Profile reading and printing */

#define BUFFER_SIZE 1024

extern char __section_program_buffer[BUFFER_SIZE];
static char *__section_program_p = __section_program_buffer;

static size_t reset_alloc(void)
{
    const size_t n = __section_program_p - __section_program_buffer;
    __section_program_p = __section_program_buffer;

    return n;
}

static char *alloc(size_t n)
{
    /* Round to multiple of 4 if necessary, to keep addresses
     * aligned. */

    n = (n + 3) & ~3;

    if (__section_program_p + n - __section_program_buffer > BUFFER_SIZE) {
        return NULL;
    }

    char * const p = __section_program_p;
    __section_program_p += n;

    return p;
}

static const char *buffer;

static bool print_error_callback(void)
{
    uprintf("...%s\n", buffer);

    return true;
}

#define PEEK() (*buffer)
#define CHECK(X) ((*buffer == (X)) && buffer++)

#define ERROR() {                                               \
        uprintf("! %d\n", __LINE__);                            \
        add_callback(print_error_callback, tick_callbacks);     \
        return 0;                                               \
    }

#define EXPECT(X) {                             \
        if (*buffer != (X)) {                   \
            ERROR();                            \
        } else {                                \
            buffer++;                           \
        }                                       \
    }

#define READ_INT(X) X = strtol(buffer, (char **)&buffer, 10)
#define READ_DOUBLE(_F) {                               \
        const char *t = buffer;                         \
        double f = strtod(buffer, (char **)&buffer);    \
        _F = (buffer == t) ? (double)NAN : f;           \
    }

#define READ_CHAR(_C) _C = *(buffer++);

static size_t read_stage(size_t i, struct stage **stages)
{
    if (!CHECK(',')) {
        EXPECT('\0');

        *stages = (struct stage *)alloc(i * sizeof(struct stage));

        return i;
    }

    struct stage stage;
    char c;

    /* Input */

    READ_CHAR(c);

    switch(c) {
    case 'a': stage.input_mode = ABSOLUTE; break;
    case 'q': stage.input_mode = RATIOMETRIC; break;
    case 'r': stage.input_mode = RELATIVE; break;
    default: ERROR();
    }

    READ_CHAR(c);

    switch(c) {
    case 'p': stage.input = PRESSURE_INPUT; break;
    case 't': stage.input = TIME_INPUT; break;
    case 'v': stage.input = VOLUME_INPUT; break;
    case 'm': stage.input = MASS_INPUT; break;
    default: ERROR();
    }

    EXPECT(';');

    /* Output */

    READ_CHAR(c);

    switch(c) {
    case 'a': stage.output_mode = ABSOLUTE; break;
    case 'r': stage.output_mode = RELATIVE; break;
    case 'q': stage.output_mode = RATIOMETRIC; break;
    default: ERROR();
    }

    READ_CHAR(c);

    switch(c) {
    case 'f': stage.output = FLOW_OUTPUT; break;
    case 'w': stage.output = POWER_OUTPUT; break;
    case 'p': stage.output = PRESSURE_OUTPUT; break;
    default: ERROR();
    }

    EXPECT(';');

    /* Points */

    stage.points = (double (*)[4])alloc(0);

    int j;

    for (j = 0; CHECK('(') ; j++) {
        double x, y;

        READ_DOUBLE(x);
        EXPECT(',');
        READ_DOUBLE(y);

        EXPECT(')');
        EXPECT(';');

        alloc(sizeof(double[4]));

        stage.points[j][0] = x;
        stage.points[j][1] = y;

        if (j == 0) {
            stage.ease_in = isnan(y);
        }
    }

    stage.sizes[0] = j;

    /* Actions */

    stage.actions = (enum action *)alloc(0);

    for (j = 0; isalpha((int)PEEK()); j++) {
        READ_CHAR(c);

        switch(c) {
        case 'v': stage.actions[j] = RESET_VOLUME; break;
        case 't': stage.actions[j] = RESET_TIME; break;
        case 'm': stage.actions[j] = RESET_MASS; break;
        default: ERROR();
        }

        alloc(sizeof(enum action));
    }

    stage.sizes[1] = j;

    /* Read next stage, then save. */

    const size_t n = read_stage(i + 1, stages);

    if (n > 0) {
        memcpy(*stages + i, &stage, sizeof(struct stage));
    }

    return n;
}

#undef PEEK
#undef CHECK
#undef EXPECT
#undef READ_INT
#undef READ_DOUBLE
#undef READ_CHAR

const struct profile *get_profile(void)
{
    return &profile;
}

bool read_profile(const char *s)
{
    struct profile p;

    buffer = s;

    /* Stages */

    p.size = read_stage(0, &p.stages);
    p.alloc = reset_alloc();

    if (p.size > 0) {
        profile = p;
        interpolate_profile();

        return true;
    }

    return false;
}

void print_profile(void)
{
    for (size_t i = 0; i < profile.size; i++) {
        const struct stage *stage = profile.stages + i;

        if (i > 0) {
            uprintf(",");
        }

        /* Input */

        switch(stage->input_mode) {
        case ABSOLUTE: uprintf("a"); break;
        case RATIOMETRIC: uprintf("q"); break;
        case RELATIVE: uprintf("r"); break;
        }

        switch(stage->input) {
        case PRESSURE_INPUT: uprintf("p"); break;
        case TIME_INPUT: uprintf("t"); break;
        case VOLUME_INPUT: uprintf("v"); break;
        case MASS_INPUT: uprintf("m"); break;
        }

        uprintf(";");

        /* Output */

        switch(stage->output_mode) {
        case ABSOLUTE: uprintf("a"); break;
        case RATIOMETRIC: uprintf("q"); break;
        case RELATIVE: uprintf("r"); break;
        }

        switch(stage->output) {
        case FLOW_OUTPUT: uprintf("f"); break;
        case POWER_OUTPUT: uprintf("w"); break;
        case PRESSURE_OUTPUT: uprintf("p"); break;
        }

        uprintf(";");

        /* Points */

        for (size_t j = 0; j < stage->sizes[0]; j++) {
            if (stage->ease_in && j == 0) {
                uprintf("(%f,);", (double)stage->points[j][0]);
            } else {
                uprintf(
                    "(%f,%f);",
                    (double)stage->points[j][0], (double)stage->points[j][1]);
            }
        }

        /* Actions */

        for (size_t j = 0; j < stage->sizes[1]; j++) {
            switch(stage->actions[j]) {
            case RESET_VOLUME: uprintf("v"); break;
            case RESET_TIME: uprintf("t"); break;
            case RESET_MASS: uprintf("m"); break;
            }
        }
    }

    uprintf("\n");
}
