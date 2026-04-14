#ifndef I2C_H
#define I2C_H

#include <stdint.h>

void i2c_init(void);
int i2c_write(uint8_t addr, const uint8_t *data, uint32_t len);

#endif /* I2C_H */
