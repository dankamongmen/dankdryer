#include "bme680.h"
#include <esp_log.h>
#include <freertos/FreeRTOS.h>

static const char* TAG = "bme";

enum {
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

// temp, pressure, and humidity are oversampling settings, or 0 to skip the
// relevant measurements. oversampling can be done at x1, x2, x4, x8, or x16.
static int
bme680_set_measurements(i2c_master_dev_handle_t i2c, unsigned temp,
                        unsigned pressure, unsigned humidity){
  uint8_t buf[] = {
    BME680_REG_CTRL_MEAS,
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
  // set up osrs_t and osrs_p
  buf[1] = (tbits << 5u) | (pbits << 2u);
  if(bme680_xmit(i2c, buf, sizeof(buf))){
    return -1;
  }
  // set up osrs_h
  buf[0] = BME680_REG_CTRL_HUM;
  buf[1] = hbits;
  if(bme680_xmit(i2c, buf, sizeof(buf))){
    return -1;
  }
  ESP_LOGI(TAG, "configured measurements with 0x%02x", buf[1]);
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
  if(bme680_set_measurements(i2c, 16, 16, 16)){
    return -1;
  }
  return 0;
}

int bme680_temp(i2c_master_dev_handle_t i2c, uint32_t *temp){
  *temp = ~0UL;
  bool ready;
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
  return 0;
}

int bme680_pressure(i2c_master_dev_handle_t i2c, uint32_t* pressure){
  *pressure = ~0UL;
  bool ready;
  if(bme680_data_ready(i2c, &ready)){
    return -1;
  }
  if(!ready){
    ESP_LOGE(TAG, "measurement data wasn't ready");
    return -1;
  }
  uint8_t buf[] = {
    BME680_REG_PRESSURE_MSB
  };
  uint8_t rbuf;
  esp_err_t e;
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
