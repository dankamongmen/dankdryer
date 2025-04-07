#ifndef HOHLRAUM_EFUSE
#define HOHLRAUM_EFUSE

#include <stdint.h>
#include <stdbool.h>

// LL-DDD-DDD-DDD. need 15 bytes for a buffer.
#define DEVICEIDLEN 15

// returns 0 on success, -1 on an invalid device id/serial.
int load_device_id(void);

// returns true iff we've loaded a valid device ID.
bool deviceid_configured(void);

// returns the serial number as extracted from the device ID.
// this will be zero if deviceid_configured() returns false.
uint32_t get_serial_num(void);

// does the hardware implementation use the (old) LM35 thermometer?
bool electronics_use_lm35(void);

// set the device id, writing it to the eFuse, iff devid is valid.
// otherwise (and on eFuse error) return -1.
int set_device_id(const unsigned char* devid);

#endif
