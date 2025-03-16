#ifndef HOHLRAUM_RESET
#define HOHLRAUM_RESET

#include <stdint.h>
#include <driver/gpio.h>

// install an interrupt handler on the specified pin. when it goes low,
// it saves the time. when it goes high, it marks the time with a
// sentinel value.
int setup_factory_reset(gpio_num_t pin);

// check to see if the reset pin is low, and has been low for more than
// the hold time. if so, return true.
bool check_factory_reset(int64_t curtime);

#endif
