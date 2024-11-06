#include "bme680.h"
#include <esp_log.h>
#include <freertos/FreeRTOS.h>

static const char* TAG = "bme";

enum {
  BME680_IIR_0 = 0,
  BME680_IIR_1 = 1,
  BME680_IIR_3 = 2,
  BME680_IIR_7 = 3,
  BME680_IIR_15 = 4,
  BME680_IIR_31 = 5,
  BME680_IIR_63 = 6,
  BME680_IIR_127 = 7,
};

typedef enum {
  BME680_REG_MEAS_STATUS = 0x1d,
  BME680_REG_PRESSURE_MSB = 0x1f,
  BME680_REG_PRESSURE_LSB = 0x20,
  BME680_REG_PRESSURE_XLSB = 0x21,
  BME680_REG_TEMP_MSB = 0x22,
  BME680_REG_TEMP_LSB = 0x23,
  BME680_REG_TEMP_XLSB = 0x24,
  BME680_REG_HUM_MSB = 0x25,
  BME680_REG_HUM_LSB = 0x26,
  BME680_REG_GAS_R_MSB = 0x2a,
  BME680_REG_GAS_R_LSB = 0x2b,
  BME680_REG_CTRL_GAS0 = 0x70,
  BME680_REG_CTRL_GAS1 = 0x71,
  BME680_REG_CTRL_HUM = 0x72,
  BME680_REG_CTRL_MEAS = 0x74,
  BME680_REG_CONFIG = 0x75,
  BME680_REG_ID = 0xd0,
  BME680_REG_RESET = 0xe0,
} registers;

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

// get the single byte of some register
static inline int
bme680_readreg(i2c_master_dev_handle_t i2c, registers reg,
               const char* regname, uint8_t* val){
  uint8_t r = reg;
  esp_err_t e;
  if((e = i2c_master_transmit_receive(i2c, &r, 1, val, 1, TIMEOUT_MS)) != ESP_OK){
    ESP_LOGE(TAG, "error (%s) requesting %s via I2C", esp_err_to_name(e), regname);
    return -1;
  }
  ESP_LOGD(TAG, "got %s: 0x%02x", regname, *val);
  return 0;
}

static inline int
bme680_control_measurements(i2c_master_dev_handle_t i2c, uint8_t* val){
  return bme680_readreg(i2c, BME680_REG_CTRL_MEAS, "CTRLMEAS", val);
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

// set the filter bits of the config register. iir must come from BME680_IIR_*.
static int
bme680_set_iirfilter(i2c_master_dev_handle_t i2c, unsigned iir){
  uint8_t buf[] = {
    BME680_REG_CONFIG,
    0x0
  };
  if(iir >= 8){
    ESP_LOGE(TAG, "invalid IIR filter %u", iir);
    return -1;
  }
  uint8_t rbuf;
  esp_err_t e = i2c_master_transmit_receive(i2c, buf, 1, &rbuf, 1, TIMEOUT_MS);
  if(e != ESP_OK){
    ESP_LOGE(TAG, "error %s reading config register", esp_err_to_name(e));
  }
  // filter is bits 4..2
  rbuf &= 0xe3;
  rbuf |= (iir << 2u);
  if(bme680_xmit(i2c, buf, sizeof(buf))){
    return -1;
  }
  ESP_LOGI(TAG, "wrote 0x%02x to config register", rbuf);
  return 0;
}

// set force mode, which ought lead to a measurement.
static int
bme680_set_forcemode(i2c_master_dev_handle_t i2c){
  uint8_t buf[] = {
    BME680_REG_CTRL_MEAS,
    0x0
  };
  uint8_t rbuf;
  esp_err_t e = i2c_master_transmit_receive(i2c, buf, 1, &rbuf, 1, TIMEOUT_MS);
  if(e != ESP_OK){
    ESP_LOGE(TAG, "error %s reading measurement control register", esp_err_to_name(e));
  }
  // mode is bits 1..0
  rbuf &= 0xfc;
  rbuf |= 0x1;
  if(bme680_xmit(i2c, buf, sizeof(buf))){
    return -1;
  }
  ESP_LOGI(TAG, "wrote 0x%02x to measurement control register", rbuf);
  return 0;
}

static int
bme680_set_gas(i2c_master_dev_handle_t i2c){
  // enable gas sampling
  uint8_t buf[] = {
    BME680_REG_CTRL_GAS1,
    0x10
  };
  if(bme680_xmit(i2c, buf, sizeof(buf))){
    return -1;
  }
  ESP_LOGI(TAG, "enabled gas with 0x%02x", buf[1]);
  return 0;
}

// temp, pressure, and humidity are oversampling settings, or 0 to skip the
// relevant measurements. oversampling can be done at x1, x2, x4, x8, or x16.
static int
bme680_set_oversampling(i2c_master_dev_handle_t i2c, unsigned temp,
                        unsigned pressure, unsigned humidity){
  uint8_t buf[] = {
    BME680_REG_CTRL_HUM,
    0x0
  };
  uint8_t tbits = 0, pbits = 0, hbits = 0;
  // check all oversampling params to ensure they are powers of 2 and <= 16
  if((temp & (temp - 1)) || (temp > 16)){
    ESP_LOGE(TAG, "invalid temp ctrl %u", temp);
    return -1;
  }
  while(temp){
    ++tbits;
    temp /= 2;
  }
  if((pressure & (pressure - 1)) || (pressure > 16)){
    ESP_LOGE(TAG, "invalid pressure ctrl %u", pressure);
    return -1;
  }
  while(pressure){
    ++pbits;
    pressure /= 2;
  }
  if((humidity & (humidity - 1)) || (humidity > 16)){
    ESP_LOGE(TAG, "invalid humidity ctrl %u", humidity);
    return -1;
  }
  while(humidity){
    ++hbits;
    humidity /= 2;
  }
  // set up osrs_h
  buf[1] = hbits;
  if(bme680_xmit(i2c, buf, sizeof(buf))){
    return -1;
  }
  ESP_LOGI(TAG, "configured humidity with 0x%02x", buf[1]);
  // FIXME check register
  // set up osrs_t and osrs_p
  buf[0] = BME680_REG_CTRL_MEAS;
  buf[1] = (tbits << 5u) | (pbits << 2u);
  if(bme680_xmit(i2c, buf, sizeof(buf))){
    return -1;
  }
  ESP_LOGI(TAG, "configured measurements with 0x%02x", buf[1]);
  uint8_t rbuf;
  if(bme680_control_measurements(i2c, &rbuf)){
    return -1;
  }
  if(((rbuf & 0xe0) != (tbits << 5u)) || ((rbuf & 0x1c) != (pbits << 2u))){
    ESP_LOGE(TAG, "unexpected ctrl_meas read 0x%02x", rbuf);
    //return -1;
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
  if(bme680_set_oversampling(i2c, 16, 16, 16)){
    return -1;
  }
  if(bme680_set_iirfilter(i2c, BME680_IIR_3)){
    return -1;
  }
  if(bme680_set_gas(i2c)){
    return -1;
  }
  if(bme680_set_forcemode(i2c)){
    return -1;
  }
  return 0;
}

int bme680_sense(i2c_master_dev_handle_t i2c, uint32_t* temp,
                 uint32_t* pressure, uint32_t* humidity){
  bool ready;
  *temp = ~0UL;
  *pressure = ~0UL;
  *humidity = ~0UL;
  if(bme680_data_ready(i2c, &ready)){
    return -1;
  }
  if(!ready){
    ESP_LOGE(TAG, "measurement data wasn't ready");
    return -1;
  }
  uint8_t buf[] = {
    BME680_REG_TEMP_MSB
  };
  uint8_t rbuf;
  esp_err_t e;
  e = i2c_master_transmit_receive(i2c, buf, 1, &rbuf, 1, TIMEOUT_MS);
  if(e != ESP_OK){
    ESP_LOGE(TAG, "error (%s) reading temp MSB", esp_err_to_name(e));
    return -1;
  }
  *temp = rbuf << 12lu;
  buf[0] = BME680_REG_TEMP_LSB;
  if((e = i2c_master_transmit_receive(i2c, buf, 1, &rbuf, 1, TIMEOUT_MS)) != ESP_OK){
    ESP_LOGE(TAG, "error (%s) reading temp LSB", esp_err_to_name(e));
    return -1;
  }
  *temp |= (rbuf << 4lu);
  buf[0] = BME680_REG_TEMP_XLSB;
  if((e = i2c_master_transmit_receive(i2c, buf, 1, &rbuf, 1, TIMEOUT_MS)) != ESP_OK){
    ESP_LOGE(TAG, "error (%s) reading temp XLSB", esp_err_to_name(e));
    return -1;
  }
  *temp |= (rbuf >> 4lu);
  ESP_LOGI(TAG, "read temperature: %lu", *temp);
  buf[0] = BME680_REG_PRESSURE_MSB;
  e = i2c_master_transmit_receive(i2c, buf, 1, &rbuf, 1, TIMEOUT_MS);
  if(e != ESP_OK){
    ESP_LOGE(TAG, "error (%s) reading pressure MSB", esp_err_to_name(e));
    return -1;
  }
  *pressure = rbuf << 12lu;
  buf[0] = BME680_REG_PRESSURE_LSB;
  if((e = i2c_master_transmit_receive(i2c, buf, 1, &rbuf, 1, TIMEOUT_MS)) != ESP_OK){
    ESP_LOGE(TAG, "error (%s) reading pressure LSB", esp_err_to_name(e));
    return -1;
  }
  *pressure |= (rbuf << 4lu);
  buf[0] = BME680_REG_PRESSURE_XLSB;
  if((e = i2c_master_transmit_receive(i2c, buf, 1, &rbuf, 1, TIMEOUT_MS)) != ESP_OK){
    ESP_LOGE(TAG, "error (%s) reading pressure XLSB", esp_err_to_name(e));
    return -1;
  }
  *pressure |= (rbuf >> 4lu);
  ESP_LOGI(TAG, "read pressure: %lu", *pressure);
  buf[0] = BME680_REG_HUM_MSB;
  e = i2c_master_transmit_receive(i2c, buf, 1, &rbuf, 1, TIMEOUT_MS);
  if(e != ESP_OK){
    ESP_LOGE(TAG, "error (%s) reading humidity MSB", esp_err_to_name(e));
    return -1;
  }
  *humidity = rbuf << 8lu;
  buf[0] = BME680_REG_HUM_LSB;
  if((e = i2c_master_transmit_receive(i2c, buf, 1, &rbuf, 1, TIMEOUT_MS)) != ESP_OK){
    ESP_LOGE(TAG, "error (%s) reading humidity LSB", esp_err_to_name(e));
    return -1;
  }
  *humidity |= rbuf;
  ESP_LOGI(TAG, "read humidity: %lu", *humidity);
  return 0;
}

int bme680_data_ready(i2c_master_dev_handle_t i2c, bool* ready){
  uint8_t buf[] = {
    BME680_REG_MEAS_STATUS
  };
  uint8_t rbuf;
  esp_err_t e = i2c_master_transmit_receive(i2c, buf, 1, &rbuf, 1, TIMEOUT_MS);
  if(e != ESP_OK){
    ESP_LOGE(TAG, "error (%s) reading measure status register", esp_err_to_name(e));
    return -1;
  }
  ESP_LOGI(TAG, "status register reported 0x%02x", rbuf);
  *ready = !!(rbuf & 0x8u);
  return 0;
}
