#include "efuse.h"
#include "esp_efuse.h"
#include <ctype.h>

#define TAG "efuse"

// different hardware, encoded into the device ID
typedef enum {
  ELECTRONICS_UNDEFINED, // who knows
  ELECTRONICS_DEVKIT,    // esp32c6 devkitC
  ELECTRONICS_PCB200,    // PCB version 2.0.0 (LM35 thermostat)
  ELECTRONICS_PCB220,    // PCB version 2.2.0 (LMT87LPT thermostat)
} electronics_e;

// eFuse provides a set of write-once 256-bit blocks, each composed of
// 8 32-bit registers. we only get one block for user data; we use the
// first two registers for the device's serial number, having the
// printed form LL-DDD-DDD-DDD where L is any capital letter and DDD
// is any number between 0 and 999 inclusive.
typedef struct device_id {
  unsigned char product[2];      // product ID (two letters)
  electronics_e elec;
  uint32_t serialnum;   // serial number 0--999999
} device_id;

static device_id DeviceID;

int load_device_id(void){
  device_id* did = &DeviceID;
  uint32_t r0 = esp_efuse_read_reg(EFUSE_BLK_USER_DATA, 0);
  // first register is the product code, represented by two
  // capital letters. 0 is uninitialized. written value is
  // 1..676 inclusive.
  if(r0 > 26 * 26){
    ESP_LOGE(TAG, "invalid product id: %" PRIu32);
    return -1;
  }
  uint32_t r1 = esp_efuse_read_reg(EFUSE_BLK_USER_DATA, 1);
  // second register is the serial number, represented by three sets of three
  // base-10 digits. 0 is uninitialized. written value is 1..1B inclusive.
  if(r1 > 1000ul * 1000ul * 1000ul){
    ESP_LOGE(TAG, "invalid serial number: %" PRIu32);
    return -1;
  }
  // either both must be zero, or neither can be zero
  if(!!r0 != !!r1){
    ESP_LOGE(TAG, "invalid prod/serial combo: %" PRIu32 " %" PRIu32);
    return -1;
  }
  did->elec = ELECTRONICS_UNDEFINED;
  if(r0 == 0){
    ESP_LOGI(TAG, "uninitialized product id/serial");
    did->product[0] = 'x';
    did->product[1] = 'x';
    did->serialnum = 0;
  }else{
    r0 = r0 - 1;
    r1 = r1 - 1;
    did->product[1] = 'A' + (r0 % 26);
    did->product[0] = 'A' + (r0 / 26);
    uint32_t elec = r1 / 1000000ul;
    switch(elec){
      case 100:
        did->elec = ELECTRONICS_DEVKIT;
        break;
      case 200:
        did->elec = ELECTRONICS_PCB200;
        break;
      case 220:
        did->elec = ELECTRONICS_PCB220;
        break;
      default:
        ESP_LOGE(TAG, "invalid electronics %" PRIu32, elec);
        break;
    }
    did->serialnum = r1 % 1000000ul;
  }
  ESP_LOGI(TAG, "product id: %c%c serial: %03u-%03u-%03u", did->product[0], did->product[1],
                did->elec, did->serialnum / 1000, did->serialnum % 1000);
  return 0;
}

int set_device_id(const char* devid){
  // FIXME lex, validate, and convert
  // FIXME write to eFuse
  return 0;
}

bool deviceid_configured(void){
  // return false for both uninitialized zeroes and 'x'
  return isupper(DeviceID.product[0]) && isupper(DeviceID.product[1]);
}

uint32_t get_serial_num(void){
  return DeviceID.serialnum;
}

// only PCBs <= 2.0.0 use the LM35 thermometer
bool electronics_use_lm35(void){
  return DeviceID.elec == ELECTRONICS_PCB200;
}
