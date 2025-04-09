#ifndef HOHLRAUM_HEATER
#define HOHLRAUM_HEATER

#include <time.h>
#include <stdbool.h>
#include <driver/gpio.h>
#include <esp_adc/adc_oneshot.h>

#define MIN_TEMP -80
#define MAX_TEMP 200

static inline bool
temp_valid_p(float temp){
  return temp >= MIN_TEMP && temp <= MAX_TEMP;
}

int setup_temp(gpio_num_t thermpin, adc_unit_t unit);

bool get_heater_state(void);
void set_heater(gpio_num_t pin, bool enabled);
float manage_heater(gpio_num_t ssrpin, time_t dryends, uint32_t targtemp);

// get the last sampled temperature of the hot chamber (does not itself
// trigger a sampling; that takes place in manage_heater()).
float get_upper_temp(void);

#endif
