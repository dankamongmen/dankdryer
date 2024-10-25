#include "nau7802.h"
#include <esp_log.h>
#include <freertos/FreeRTOS.h>

static const char* TAG = "nau";

//Bits within the PU_CTRL register
typedef enum {
  NAU7802_PU_CTRL_RR = 1,
  NAU7802_PU_CTRL_PUD = 2,
  NAU7802_PU_CTRL_PUA = 4,
  NAU7802_PU_CTRL_PUR = 8,
  NAU7802_PU_CTRL_CS = 16,
  NAU7802_PU_CTRL_CR = 32,
  NAU7802_PU_CTRL_OSCS = 64,
  NAU7802_PU_CTRL_AVDDS = 128,
} PU_CTRL_Bits;

typedef enum {
  NAU7802_PU_CTRL = 0x00,
  NAU7802_CTRL1,
  NAU7802_CTRL2,
  NAU7802_OCAL1_B2,
  NAU7802_OCAL1_B1,
  NAU7802_OCAL1_B0,
  NAU7802_GCAL1_B3,
  NAU7802_GCAL1_B2,
  NAU7802_GCAL1_B1,
  NAU7802_GCAL1_B0,
  NAU7802_OCAL2_B2,
  NAU7802_OCAL2_B1,
  NAU7802_OCAL2_B0,
  NAU7802_GCAL2_B3,
  NAU7802_GCAL2_B2,
  NAU7802_GCAL2_B1,
  NAU7802_GCAL2_B0,
  NAU7802_I2C_CONTROL,
  NAU7802_ADCO_B2,
  NAU7802_ADCO_B1,
  NAU7802_ADCO_B0,
  NAU7802_ADC = 0x15, //Shared ADC and OTP 32:24
  NAU7802_OTP_B1,     //OTP 23:16 or 7:0?
  NAU7802_OTP_B0,     //OTP 15:8
  NAU7802_PGA = 0x1B,
  NAU7802_PGA_PWR = 0x1C,
  NAU7802_DEVICE_REV = 0x1F,
} Scale_Registers;

#define NAU7802_ADDRESS 0x2A
#define TIMEOUT_MS 1000 // FIXME

int nau7802_detect(i2c_master_bus_handle_t i2c, i2c_master_dev_handle_t* i2cnau){
  const unsigned addr = NAU7802_ADDRESS;
  esp_err_t e = i2c_master_probe(i2c, addr, -1);
  if(e != ESP_OK){
    ESP_LOGW(TAG, "error %d detecting NAU7802 at 0x%02x", e, addr);
    return -1;
  }
  i2c_device_config_t devcfg = {
    .dev_addr_length = I2C_ADDR_BIT_LEN_7,
    .device_address = addr,
    .scl_speed_hz = 100000,
	};
  ESP_LOGI(TAG, "successfully detected NAU7802 at 0x%02x", addr);
  if((e = i2c_master_bus_add_device(i2c, &devcfg, i2cnau)) != ESP_OK){
    ESP_LOGW(TAG, "error %d adding nau7802 i2c device", e);
    return -1;
  }
  return 0;
}

// FIXME we'll probably want this to be async
static int
nau7802_xmit(i2c_master_dev_handle_t i2c, const void* buf, size_t blen){
  esp_err_t e = i2c_master_transmit(i2c, buf, blen, TIMEOUT_MS);
  if(e != ESP_OK){
    ESP_LOGW(TAG, "error %d transmitting %zuB via I2C", e, blen);
    return -1;
  }
  return 0;
}

int nau7802_reset(i2c_master_dev_handle_t i2c){
  uint8_t buf[] = {
    NAU7802_PU_CTRL,
    NAU7802_PU_CTRL_RR
  };
  if(nau7802_xmit(i2c, buf, sizeof(buf))){
    return -1;
  }
  ESP_LOGI(TAG, "successfully reset NAU7802");
  return 0;
}

int nau7802_poweron(i2c_master_dev_handle_t i2c){
  uint8_t buf[] = {
    NAU7802_PU_CTRL,
    NAU7802_PU_CTRL_PUD | NAU7802_PU_CTRL_PUA
  };
  if(nau7802_xmit(i2c, buf, sizeof(buf))){
    return -1;
  }
  uint8_t rbuf[2];
  vTaskDelay(pdMS_TO_TICKS(200));
  esp_err_t e;
  if((e = i2c_master_transmit_receive(i2c, buf, sizeof(buf) - 1, rbuf, sizeof(rbuf), -1)) != ESP_OK){
    ESP_LOGW(TAG, "error %d requesting data via I2C", e);
    return -1;
  }
  if(!(rbuf[0] & NAU7802_PU_CTRL_PUR)){
    ESP_LOGW(TAG, "didn't see powered-on bit");
    return -1;
  }
  buf[1] = rbuf[0] | NAU7802_PU_CTRL_CS;
  if(nau7802_xmit(i2c, buf, sizeof(buf))){
    return -1;
  }
  ESP_LOGI(TAG, "successfully started NAU7802 cycle");
  buf[0] = NAU7802_DEVICE_REV;
  e = i2c_master_transmit_receive(i2c, buf, sizeof(buf) - 1, rbuf, sizeof(rbuf), -1);
  if(e != ESP_OK){
    ESP_LOGW(TAG, "error %d reading device revision code", e);
    return -1;
  }
  ESP_LOGI(TAG, "device revision code: 0x%02x 0x%02x", rbuf[0], rbuf[1]);
  return 0;
}
