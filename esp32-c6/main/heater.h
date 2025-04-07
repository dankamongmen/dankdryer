#ifndef HOHLRAUM_HEATER
#define HOHLRAUM_HEATER

#include <stdbool.h>

bool get_heater_state(void);
void set_heater(bool enabled);

#endif
