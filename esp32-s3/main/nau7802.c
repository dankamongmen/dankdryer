#include "nau7802.h"

#define NAU7802_ADDRESS 0x2A

int nau7802_detect(i2c_master_bus_handle_t i2c, bool* present){
  esp_err_t e = i2c_master_probe(i2c, NAU7802_ADDRESS, -1);
  if(e != ESP_OK){
    *present = false;
    return -1;
  }
  *present = true;
  return 0;
}


