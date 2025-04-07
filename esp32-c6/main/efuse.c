#include "efuse.h"
#include "esp_efuse.h"

#define TAG "efuse"

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
  if(r0 == 0){
    ESP_LOGI(TAG, "uninitialized product id/serial");
    did->product[0] = 'x';
    did->product[1] = 'x';
    did->electronics = 0;
    did->serialnum = 0;
  }else{
    r0 = r0 - 1;
    r1 = r1 - 1;
    did->product[1] = 'A' + (r0 % 26);
    did->product[0] = 'A' + (r0 / 26);
    did->electronics = r1 / 1000000ul;
    did->serialnum = r1 % 1000000ul;
  }
  ESP_LOGI(TAG, "product id: %c%c serial: %03u-%03u-%03u", did->product[0], did->product[1],
                did->electronics, did->serialnum / 1000, did->serialnum % 1000);
  return 0;
}

uint32_t get_serial_num(void){
  return DeviceID.serialnum;
}
