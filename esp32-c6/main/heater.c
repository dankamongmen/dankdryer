#include "dankdryer.h"
#include "heater.h"
#include "pins.h"
#include <esp_log.h>

#define TAG "therm"

static bool HeaterState;

bool get_heater_state(void){
  return HeaterState;
}

void set_heater(gpio_num_t pin, bool enabled){
  HeaterState = enabled;
  gpio_level(pin, enabled);
  ESP_LOGI(TAG, "set heater %s", bool_as_onoff(HeaterState));
}
