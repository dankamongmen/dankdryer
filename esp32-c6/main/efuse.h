#ifndef HOHLRAUM_EFUSE
#define HOHLRAUM_EFUSE

#include <stdint.h>
#include <stdbool.h>

// returns 0 on success, -1 on an invalid device id/serial.
int load_device_id(void);

uint32_t get_serial_num(void);

// does the hardware implementation use the (old) LM35 thermometer?
bool electronics_use_lm35(void);

#endif
