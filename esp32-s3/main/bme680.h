#ifndef DANKAMONGMEN_BME680
#define DANKAMONGMEN_BME680

// ESP-IDF component for working with Bosch BME680 environmental sensors.

#include <driver/i2c_master.h>

// Probe the I2C bus specified by i2c for a BME680. If it is found, the device
// handle i2cbme will be prepared for use, and 0 will be returned.
int bme680_detect(i2c_master_bus_handle_t i2c, i2c_master_dev_handle_t* i2cbme);

// Perform a soft reset of the device.
int bme680_reset(i2c_master_dev_handle_t i2c);

// Initialize the device, making it suitable for use.
int bme680_init(i2c_master_dev_handle_t i2c);

// Is data ready? Reading measurements can fail otherwise.
int bme680_data_ready(i2c_master_dev_handle_t i2c, bool* ready);

// Get the 20-bit temperature (low 4 bits depend on oversampling).
int bme680_temp(i2c_master_dev_handle_t i2c, uint32_t* temp);

// Get the 20-bit pressure (low 4 bits depend on oversampling).
int bme680_pressure(i2c_master_dev_handle_t i2c, uint32_t* pressure);

#endif
