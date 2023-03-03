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
