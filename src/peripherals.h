#ifndef PERIPHERALS_H
#define PERIPHERALS_H

#include "pid.h"
#include "filter.h"

void reset_power(void);
void set_heat_delay(double d);
void set_heat_power(double P);
double get_heat_delay(void);
double get_heat_power(void);
void set_pump_delay(double d);
void set_pump_power(double P);
void set_pump_flow(double Q);
double get_pump_delay(void);
double get_pump_power(void);
double get_pump_flow(void);

extern struct pid temperature_pid;
extern struct filter temperature_filter, error_filter;
void run_temperature(bool run);
void read_temperature(void);
void reset_temperature(void);

extern struct pid pressure_pid;
extern struct filter pressure_filter;
void run_pressure(bool run);
void read_pressure(void);
void reset_pressure(void);

extern struct filter mass_filter;
extern struct filter mass_rate_filter;
void run_mass(bool run);
void read_mass(void);
void tare_mass(void);
bool is_taring_mass(void);

extern struct pid flow_pid;
extern struct filter flow_filter;
void reset_flow(void);
void tare_flow(void);
double get_flow(void);
double get_flow_derivative(void);
double get_volume(void);

void reset_display(void);
void clear_display(void);
void display(const char *s, ...);

void reset_input(void);

#endif
