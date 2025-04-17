#include "pins.h"
#include <esp_log.h>

#define TAG "pins"

int gpio_level(gpio_num_t pin, bool level){
  esp_err_t e = gpio_set_level(pin, level);
  if(e != ESP_OK){
    ESP_LOGE(TAG, "error (%s) setting pin %d to %u", esp_err_to_name(e), pin, level);
    return -1;
  }
  return 0;
}
