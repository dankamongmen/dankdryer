#ifndef DANKDRYER_NAU7802
#define DANKDRYER_NAU7802

// ESP-IDF component for working with Nuvatron NAU7802 ADCs.

#include <driver/i2c_master.h>

// probe the I2C bus for an NAU7802. if it is found, configure i2cnau
// as a device handle for it, and return 0. return non-zero on error.
int nau7802_detect(i2c_master_bus_handle_t i2c, i2c_master_dev_handle_t* i2cnau);

// send the reset command w/ timeout. returns non-zero on error.
int nau7802_reset(i2c_master_dev_handle_t i2c);

// send the poweron command w/ timeout. returns non-zero on error.
int nau7802_poweron(i2c_master_dev_handle_t i2c);

// set the gain (default 1). can be any power of 2 from 1 to 128.
// confirms the set and returns 0 on success, non-zero on failure.
int nau7802_setgain(i2c_master_dev_handle_t i2c, unsigned gain);

typedef enum {
  NAU7802_LDO_24V,
  NAU7802_LDO_27V,
  NAU7802_LDO_30V,
  NAU7802_LDO_33V,
  NAU7802_LDO_36V,
  NAU7802_LDO_39V,
  NAU7802_LDO_42V,
  NAU7802_LDO_45V
} nau7802_ldo_mode;

// configure the LDO voltage and disable AVDD source.
int nau7802_setldo(i2c_master_dev_handle_t i2c, nau7802_ldo_mode mode);

// read the current weight. returns a negative number on error.
float nau7802_read(i2c_master_dev_handle_t i2c);

#define TIMEOUT_MS 1000 // FIXME why 1s?

#endif
