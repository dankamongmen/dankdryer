#ifndef DANKDRYER_NAU7802
#define DANKDRYER_NAU7802

// ESP-IDF component for working with Nuvatron NAU7802 ADCs.

#include <driver/i2c_master.h>

// probe the I2C bus for an NAU7802. if it is found, configure i2cnau
// as a device handle for it, and return 0.
int nau7802_detect(i2c_master_bus_handle_t i2c, i2c_master_dev_handle_t* i2cnau);

// send the reset command w/ timeout
int nau7802_reset(i2c_master_dev_handle_t i2c);

// send the poweron command w/ timeout
int nau7802_poweron(i2c_master_dev_handle_t i2c);

#endif
