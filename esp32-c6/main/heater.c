#include "pins.h"
#include "heater.h"

static bool HeaterState;

bool get_heater_state(void){
  return HeaterState;
}

void set_heater(bool enabled){
  HeaterState = enabled;
  gpio_level(SSR_GPIN, enabled);
  printf("set heater %d\n", HeaterState);
}
