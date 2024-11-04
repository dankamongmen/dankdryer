#include "bme680.h"
#include <esp_log.h>
#include <freertos/FreeRTOS.h>

static const char* TAG = "bme";

enum {
  BME680_REG_ID = 0xd0,
  BME680_REG_RESET = 0xe0,
};

static const unsigned TIMEOUT_MS = 1000u; // FIXME why 1s?

int bme680_detect(i2c_master_bus_handle_t i2c, i2c_master_dev_handle_t* i2cbme){
  const unsigned addr = 0x77;
  esp_err_t e = i2c_master_probe(i2c, addr, TIMEOUT_MS);
  if(e != ESP_OK){
    ESP_LOGE(TAG, "error (%s) detecting BME680 at 0x%02x", esp_err_to_name(e), addr);
    return -1;
  }
  i2c_device_config_t devcfg = {
    .dev_addr_length = I2C_ADDR_BIT_LEN_7,
    .device_address = addr,
    .scl_speed_hz = 100000,
  };
  ESP_LOGI(TAG, "successfully detected BME680 at 0x%02x", addr);
  if((e = i2c_master_bus_add_device(i2c, &devcfg, i2cbme)) != ESP_OK){
    ESP_LOGE(TAG, "error (%s) adding bme680 i2c device", esp_err_to_name(e));
    return -1;
  }
  return 0;
}

static int
bme680_xmit(i2c_master_dev_handle_t i2c, const void* buf, size_t blen){
  esp_err_t e = i2c_master_transmit(i2c, buf, blen, TIMEOUT_MS);
  if(e != ESP_OK){
    ESP_LOGE(TAG, "error (%s) transmitting %zuB via I2C", esp_err_to_name(e), blen);
    return -1;
  }
  return 0;
}

int bme680_reset(i2c_master_dev_handle_t i2c){
  uint8_t buf[] = {
    BME680_REG_RESET,
    0xb6
  };
  if(bme680_xmit(i2c, buf, sizeof(buf))){
    return -1;
  }
  ESP_LOGI(TAG, "reset BME680");
  return 0;
}

int bme680_init(i2c_master_dev_handle_t i2c){
  uint8_t buf[] = {
    BME680_REG_ID
  };
  uint8_t rbuf;
  esp_err_t e;
  e = i2c_master_transmit_receive(i2c, buf, 1, &rbuf, 1, TIMEOUT_MS);
  if(e != ESP_OK){
    ESP_LOGE(TAG, "error (%s) reading chip id", esp_err_to_name(e));
    return -1;
  }
  ESP_LOGI(TAG, "device id: 0x%02x", rbuf);
  return 0;
}
