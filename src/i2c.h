#ifndef I2C_H
#define I2C_H

void reset_i2c(void);
uint8_t read_i2c(uint8_t slave, uint8_t reg, uint8_t *buffer, size_t n);
void write_i2c(uint8_t slave, uint8_t reg, uint8_t value);
bool probe_i2c(uint8_t slave);

#endif
