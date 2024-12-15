#ifndef DANKDRYER_EMC2302
#define DANKDRYER_EMC2302

// ESP-IDF component for working with Microchip EMC2302 fan controllers.

#include <driver/i2c_master.h>

// probe the I2C bus for an EMC2302. if it is found, configure i2cemc
// as a device handle for it, and return 0. return non-zero on error.
int emc2302_detect(i2c_master_bus_handle_t i2c, i2c_master_dev_handle_t* i2cemc);

// send the reset command w/ timeout. returns non-zero on error.
int emc2302_reset(i2c_master_dev_handle_t i2c);

// send the poweron command w/ timeout. returns non-zero on error.
// it would not be unwise to call emc2302_reset() first.
// this sets PUA+PUD+CS, verifies PUR after a short delay,
// sets REG_CHPS, and sets PGA_CAP_EN, all as recommended
// by the datasheet.
int emc2302_poweron(i2c_master_dev_handle_t i2c);

// set the gain (default 1). can be any power of 2 from 1 to 128.
// confirms the set and returns 0 on success, non-zero on failure.
int emc2302_setgain(i2c_master_dev_handle_t i2c, unsigned gain);

// read the 24-bit ADC into val. returns non-zero on error, in which case
// *val is undefined. this is the raw ADC value.
int emc2302_read(i2c_master_dev_handle_t i2c, uint32_t* val);

// read the 24-bit ADC, interpreting it using some maximum value scale.
// i.e. if scale is 5000 (representing e.g. a small bar load cell of 5kg),
// the raw ADC value will be divided by 3355.4432 (1 << 24 / 5000).
// on success, val will hold some value less than scale.
int emc2302_read_scaled(i2c_master_dev_handle_t i2c, float* val, uint32_t scale);

#define TIMEOUT_MS 1000 // FIXME why 1s?

#endif
