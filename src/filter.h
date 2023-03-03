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
