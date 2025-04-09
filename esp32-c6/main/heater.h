#ifndef HOHLRAUM_HEATER
#define HOHLRAUM_HEATER

#include <stdbool.h>
#include <driver/gpio.h>

bool get_heater_state(void);
void set_heater(gpio_num_t pin, bool enabled);

#endif
