#include "efuse.h"
#include "esp_efuse.h"
#include <ctype.h>
#include <esp_system.h>

#define TAG "efuse"

#define SERIAL_REG 1

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

static electronics_e
elec_from_raw(uint16_t raw){
  ESP_LOGI(TAG, "got raw %" PRIu16, raw);
  switch(raw){
    case 100:
      return ELECTRONICS_DEVKIT;
    case 200:
      return ELECTRONICS_PCB200;
    case 220:
      return ELECTRONICS_PCB220;
    default:
      ESP_LOGE(TAG, "invalid electronics %" PRIu16, raw);
      return ELECTRONICS_UNDEFINED;
  }
}

static uint16_t
elec_to_raw(electronics_e e){
  switch(e){
    case ELECTRONICS_DEVKIT:
      return 100;
    case ELECTRONICS_PCB200:
      return 200;
    case ELECTRONICS_PCB220:
      return 220;
    default:
      break;
  }
  return 0;
}

int load_device_id(void){
  device_id* did = &DeviceID;
  uint32_t r0 = esp_efuse_read_reg(EFUSE_BLK_USER_DATA, 0);
  // first register is the product code, represented by two
  // capital letters. 0 is uninitialized. written value is
  // 1..676 inclusive.
  if(r0 > 26 * 26){
    ESP_LOGE(TAG, "invalid product id: %" PRIu32, r0);
    return -1;
  }
  uint32_t r1 = esp_efuse_read_reg(EFUSE_BLK_USER_DATA, SERIAL_REG);
  // second register is the serial number, represented by three sets of three
  // base-10 digits. 0 is uninitialized. written value is 1..1B inclusive.
  if(r1 > 1000ul * 1000ul * 1000ul){
    ESP_LOGE(TAG, "invalid serial number: %" PRIu32, r1);
    return -1;
  }
  // either both must be zero, or neither can be zero
  if(!!r0 != !!r1){
    ESP_LOGE(TAG, "invalid prod/serial combo: %" PRIu32 " %" PRIu32, r0, r1);
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
    if((did->elec = elec_from_raw(r1 / 1000000ul)) == ELECTRONICS_UNDEFINED){
      return -1;
    }
    did->serialnum = r1 % 1000000ul;
    // do this last, because deviceid_configured() decides based on these
    did->product[1] = 'A' + (r0 % 26);
    did->product[0] = 'A' + (r0 / 26);
  }
  ESP_LOGI(TAG, "product id: %c%c serial: %03u-%03u-%03u", did->product[0], did->product[1],
                did->elec, did->serialnum / 1000, did->serialnum % 1000);
  return 0;
}

static int
write_device_id(const device_id* did){
  uint32_t r;
  esp_err_t e;
  if((e = esp_efuse_batch_write_begin()) != ESP_OK){
    ESP_LOGE(TAG, "error (%s) beginning eFuse write", esp_err_to_name(e));
    return -1;
  }
  r = (did->product[0] - 'A') * 26 + (did->product[1] - 'A') + 1;
  ESP_LOGI(TAG, "writing %" PRIu32 " to register 0", r);
  if((e = esp_efuse_write_reg(EFUSE_BLK_USER_DATA, 0, r)) != ESP_OK){
    ESP_LOGE(TAG, "error (%s) writing register 0", esp_err_to_name(e));
    esp_efuse_batch_write_cancel();
    return -1;
  }
  r = elec_to_raw(did->elec) * 1000000ul + did->serialnum;
  ESP_LOGI(TAG, "writing %" PRIu32 " to register %d", r, SERIAL_REG);
  if((e = esp_efuse_write_reg(EFUSE_BLK_USER_DATA, SERIAL_REG, r)) != ESP_OK){
    ESP_LOGE(TAG, "error (%s) writing register %d", esp_err_to_name(e), SERIAL_REG);
    esp_efuse_batch_write_cancel();
    return -1;
  }
  if((e = esp_efuse_batch_write_commit()) != ESP_OK){
    ESP_LOGE(TAG, "error (%s) committing eFuse write", esp_err_to_name(e));
    return -1;
  }
  ESP_LOGW(TAG, "rebooting");
  esp_restart();
  return -1; // ought not reach here
}

int set_device_id(const unsigned char* devid){
  if(deviceid_configured()){
    ESP_LOGE(TAG, "won't rewrite device ID");
    return -1;
  }
  device_id d;
  enum {
    WANT_PID,
    WANT_D1,
    WANT_D2,
    WANT_D3,
    WANT_DONE
  } state_e = WANT_PID;
  while(*devid){
    switch(state_e){
      case WANT_PID:
        if(devid[0] != 'D' || devid[1] != 'D' || devid[2] != '-'){
          goto badlex;
        }
        d.product[0] = devid[0];
        d.product[1] = devid[1];
        devid += 3;
        state_e = WANT_D1;
        break;
      case WANT_D1:
        if(!isdigit(devid[0]) || !isdigit(devid[1]) || !isdigit(devid[2]) || devid[3] != '-'){
          goto badlex;
        }
        uint16_t elec = (devid[0] - '0') * 100 + (devid[1] - '0') * 10 + (devid[2] - '0');
        d.elec = elec_from_raw(elec);
        if(d.elec == ELECTRONICS_UNDEFINED){
          goto badlex;
        }
        devid += 4;
        state_e = WANT_D2;
        break;
      case WANT_D2:
        if(!isdigit(devid[0]) || !isdigit(devid[1]) || !isdigit(devid[2]) || devid[3] != '-'){
          goto badlex;
        }
        d.serialnum = (devid[0] - '0') * 100 + (devid[1] - '0') * 10 + (devid[2] - '0');
        devid += 4;
        state_e = WANT_D3;
        break;
      case WANT_D3:
        if(!isdigit(devid[0]) || !isdigit(devid[1]) || !isdigit(devid[2])){
          goto badlex;
        }
        d.serialnum *= 1000;
        d.serialnum += (devid[0] - '0') * 100 + (devid[1] - '0') * 10 + (devid[2] - '0');
        devid += 3;
        state_e = WANT_DONE;
        break;
      case WANT_DONE:
        ESP_LOGE(TAG, "useless crap at end %u", *devid);
        goto badlex;
    }
  }
  if(state_e != WANT_DONE){
    ESP_LOGE(TAG, "bad state %d", state_e);
    goto badlex;
  }
  ESP_LOGW(TAG, "received valid device ID, writing to eFuse");
  return write_device_id(&d);

badlex:
  ESP_LOGE(TAG, "not writing invalid device ID");
  return -1;
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
