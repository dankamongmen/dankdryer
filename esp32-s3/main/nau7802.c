#include "nau7802.h"
#include <esp_log.h>

static const char* TAG = "nau";

#define NAU7802_ADDRESS 0x2A

int nau7802_detect(i2c_master_bus_handle_t i2c, bool* present){
  const unsigned addr = NAU7802_ADDRESS;
  esp_err_t e = i2c_master_probe(i2c, addr, -1);
  if(e != ESP_OK){
    ESP_LOGW(TAG, "error %d detecting NAU7802", e);
    *present = false;
    return -1;
  }
  ESP_LOGI(TAG, "successfully detected NAU7802 at %02x", addr);
  *present = true;
  return 0;
}


