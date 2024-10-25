#ifndef DANKDRYER_NAU7802
#define DANKDRYER_NAU7802

// ESP-IDF component for working with Nuvatron NAU7802 ADCs.

#include <driver/i2c_master.h>

int nau7802_detect(i2c_master_bus_handle_t i2c, bool* present);

#endif
