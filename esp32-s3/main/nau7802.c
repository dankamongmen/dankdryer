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

int nau7802_detect(i2c_master_bus_handle_t i2c, i2c_master_dev_handle_t* i2cnau){
  const unsigned addr = NAU7802_ADDRESS;
  esp_err_t e = i2c_master_probe(i2c, addr, TIMEOUT_MS);
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
  ESP_LOGI(TAG, "reset NAU7802");
  return 0;
}

// get the single byte of the PU_CTRL register
static inline int
nau7802_readreg(i2c_master_dev_handle_t i2c, Scale_Registers reg,
                const char* regname, uint8_t* val){
  uint8_t r = reg;
  esp_err_t e;
  if((e = i2c_master_transmit_receive(i2c, &r, 1, val, 1, TIMEOUT_MS)) != ESP_OK){
    ESP_LOGW(TAG, "error (%s) requesting %s via I2C", esp_err_to_name(e), regname);
    return -1;
  }
  ESP_LOGI(TAG, "got %s: 0x%02x", regname, *val);
  return 0;
}

static inline int
nau7802_pu_ctrl(i2c_master_dev_handle_t i2c, uint8_t* val){
  return nau7802_readreg(i2c, NAU7802_PU_CTRL, "PU_CTRL", val);
}

static inline int
nau7802_ctrl1(i2c_master_dev_handle_t i2c, uint8_t* val){
  return nau7802_readreg(i2c, NAU7802_CTRL1, "CTRL1", val);
}

int nau7802_poweron(i2c_master_dev_handle_t i2c){
  uint8_t buf[] = {
    NAU7802_PU_CTRL,
    NAU7802_PU_CTRL_PUD | NAU7802_PU_CTRL_PUA
  };
  if(nau7802_xmit(i2c, buf, sizeof(buf))){
    return -1;
  }
  uint8_t rbuf;
  // FIXME i think it's actually 200 microseconds, not milliseconds?
  vTaskDelay(pdMS_TO_TICKS(200));
  if(nau7802_pu_ctrl(i2c, &rbuf)){
    return -1;
  }
  if(!(rbuf & NAU7802_PU_CTRL_PUR)){
    ESP_LOGW(TAG, "didn't see powered-on bit");
    return -1;
  }
  esp_err_t e;
  buf[1] = rbuf | NAU7802_PU_CTRL_CS;
  if(nau7802_xmit(i2c, buf, sizeof(buf))){
    return -1;
  }
  ESP_LOGI(TAG, "started NAU7802 cycle");
  buf[0] = NAU7802_DEVICE_REV;
  e = i2c_master_transmit_receive(i2c, buf, 1, &rbuf, 1, TIMEOUT_MS);
  if(e != ESP_OK){
    ESP_LOGW(TAG, "error %d reading device revision code", e);
    return -1;
  }
  ESP_LOGI(TAG, "device revision code: 0x0%x", rbuf & 0xf);
  return 0;
}

int nau7802_setgain(i2c_master_dev_handle_t i2c, unsigned gain){
  if(gain > 128 || gain == 0 || (gain & (gain - 1))){
    ESP_LOGW(TAG, "illegal gain value %u", gain);
    return -1;
  }
  uint8_t buf[] = {
    NAU7802_CTRL1,
    0xff
  };
  uint8_t rbuf;
  if(nau7802_ctrl1(i2c, &rbuf)){
    return -1;
  }
  buf[1] = rbuf & 0xf8;
  if(gain >= 16){
    buf[1] |= 0x4;
  }
  if(gain > 32 || (gain < 16 && gain > 2)){
    buf[1] |= 0x2;
  }
  if(gain == 128 || gain == 32 || gain == 8 || gain == 2){
    buf[1] |= 0x1;
  }
  esp_err_t e = i2c_master_transmit_receive(i2c, buf, 2, &rbuf, 1, TIMEOUT_MS);
  if(e != ESP_OK){
    ESP_LOGW(TAG, "error %s writing CTRL1", esp_err_to_name(e));
    return -1;
  }
  if(rbuf != buf[1]){
    ESP_LOGW(TAG, "CTRL1 reply 0x%02x didn't match 0x%02x", rbuf, buf[1]);
    return -1;
  }
  ESP_LOGI(TAG, "set gain");
  return 0;
}

int nau7802_setldo(i2c_master_dev_handle_t i2c, nau7802_ldo_mode mode){
  if(mode < NAU7802_LDO_24V || mode > NAU7802_LDO_45V){
    ESP_LOGW(TAG, "illegal LDO mode %d", mode);
    return -1;
  }
  uint8_t buf[] = {
    NAU7802_CTRL1,
    0xff
  };
  uint8_t rbuf;
  if(nau7802_ctrl1(i2c, &rbuf)){
    return -1;
  }
  buf[1] = rbuf & 0xc7;
  buf[1] |= mode << 3u;
  esp_err_t e = i2c_master_transmit_receive(i2c, buf, 2, &rbuf, 1, TIMEOUT_MS);
  if(e != ESP_OK){
    ESP_LOGW(TAG, "error (%s) writing CTRL1", esp_err_to_name(e));
    return -1;
  }
  if(rbuf != buf[1]){
    ESP_LOGW(TAG, "CTRL1 reply 0x%02x didn't match 0x%02x", rbuf, buf[1]);
    return -1;
  }
  buf[0] = NAU7802_PU_CTRL;
  if((e = i2c_master_transmit_receive(i2c, buf, 1, &rbuf, 1, TIMEOUT_MS)) != ESP_OK){
    ESP_LOGW(TAG, "error (%s) requesting data via I2C", esp_err_to_name(e));
    return -1;
  }
  buf[1] = rbuf | 0x80;
  if((e = i2c_master_transmit_receive(i2c, buf, sizeof(buf), &rbuf, 1, TIMEOUT_MS)) != ESP_OK){
    ESP_LOGW(TAG, "error (%s) requesting data via I2C", esp_err_to_name(e));
    return -1;
  }
  if(rbuf != buf[1]){
    ESP_LOGW(TAG, "PU_CTRL reply 0x%02x didn't match 0x%02x", rbuf, buf[1]);
    return -1;
  }
  ESP_LOGI(TAG, "set ldo");
  return 0;
}

float nau7802_read(i2c_master_dev_handle_t i2c){
  uint8_t reg = NAU7802_PU_CTRL;
  uint8_t r0[2], r1[2], r2[2];
  esp_err_t e = i2c_master_transmit_receive(i2c, &reg, 1, r2, 2, TIMEOUT_MS);
  if(e != ESP_OK){
    ESP_LOGW(TAG, "error %s checking CR", esp_err_to_name(e));
    return -1.0;
  }
  if(!(r2[0] & NAU7802_PU_CTRL_CR)){
    ESP_LOGW(TAG, "data not yet ready at ADC (0x%02x)", r2[0]);
    // FIXME retry?
    return -1.0;
  }
  reg = NAU7802_ADCO_B2;
  e = i2c_master_transmit_receive(i2c, &reg, 1, r2, 2, TIMEOUT_MS);
  if(e != ESP_OK){
    ESP_LOGW(TAG, "error %s reading ADC2", esp_err_to_name(e));
    return -1.0;
  }
  reg = NAU7802_ADCO_B1;
  e = i2c_master_transmit_receive(i2c, &reg, 1, r1, 2, TIMEOUT_MS);
  if(e != ESP_OK){
    ESP_LOGW(TAG, "error %s reading ADC1", esp_err_to_name(e));
    return -1.0;
  }
  reg = NAU7802_ADCO_B0;
  e = i2c_master_transmit_receive(i2c, &reg, 1, r0, 2, TIMEOUT_MS);
  if(e != ESP_OK){
    ESP_LOGW(TAG, "error %s reading ADC0", esp_err_to_name(e));
    return -1.0;
  }
  uint32_t full = (r0[1] << 16u) + (r1[1] << 8u) + r2[1];
  ESP_LOGI(TAG, "ADC reads: %u:%u %u:%u %u:%u full %lu",
           r0[1], r0[0], r1[1], r1[0], r2[1], r2[0], full);
  return full;
}
