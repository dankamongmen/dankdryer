#include "hx711.h"
#include <esp_log.h>
#include <driver/gpio.h>
#include <rom/ets_sys.h>
#include <freertos/FreeRTOS.h>

static const char TAG[] = "hx711";

static const unsigned CLOCK_DELAY_US = 20;

typedef enum HX711_GAIN {
	eGAIN_128 = 1,
	eGAIN_64 = 3,
	eGAIN_32 = 2
} HX711_GAIN;

// gpio_reset_pin() disables input and output, selects for GPIO, and enables
// pullup (we never want pullup).
static int
gpio_setup(gpio_num_t pin, gpio_mode_t mode, const char *mstr){
  gpio_reset_pin(pin);
  esp_err_t err;
  if((err = gpio_set_direction(pin, mode)) != ESP_OK){
    ESP_LOGE(TAG, "error (%s) setting %d to %s", esp_err_to_name(err), pin, mstr);
    return -1;
  }
  err = gpio_set_pull_mode(pin, GPIO_FLOATING);
  if(err != ESP_OK){
    ESP_LOGE(TAG, "error (%s) setting %d to nopull", esp_err_to_name(err), pin);
    return -1;
  }
  return 0;
}

static int
hx711_power_on(const HX711* hx){
  esp_err_t e;
  if((e = gpio_set_level(hx->clockpin, 0)) != ESP_OK){
    ESP_LOGE(TAG, "error (%s) setting hx711 clock %d low", esp_err_to_name(e), hx->clockpin);
    return -1;
  }
  return 0;
}

// affects the next reading. must be called with interrupts disabled.
static int
hx711_setgain(HX711* hx, unsigned gain){
	for (unsigned i = 0 ; i < gain ; ++i){
		gpio_set_level(hx->clockpin, 1);
		ets_delay_us(CLOCK_DELAY_US);
		gpio_set_level(hx->clockpin, 0);
		ets_delay_us(CLOCK_DELAY_US);
	}
  return 0;
}

int hx711_init(HX711 *hx, gpio_num_t dpin, gpio_num_t clockpin){
  if(gpio_setup(dpin, GPIO_MODE_INPUT, "input(np)")){
    return -1;
  }
  if(gpio_setup(clockpin, GPIO_MODE_OUTPUT, "output(np)")){
    return -1;
  }
  hx->dpin = dpin;
  hx->clockpin = clockpin;
  if(hx711_power_on(hx)){
    return -1;
  }
  portDISABLE_INTERRUPTS();
  if(hx711_setgain(hx, eGAIN_128)){
	  portENABLE_INTERRUPTS();
    return -1;
  }
	portENABLE_INTERRUPTS();
  ESP_LOGI(TAG, "using hx711 via %d and %d", hx->dpin, hx->clockpin);
  return 0;
}

static inline bool
hx711_data_ready_p(const HX711* hx){
  if(gpio_get_level(hx->dpin)){
    ESP_LOGE(TAG, "data wasn't ready");
    return false;
  }
  return true;
}

int hx711_read(HX711 *hx, int32_t* val){
  if(!hx711_data_ready_p(hx)){
    return -1;
  }
  int ret = 0;
  *val = 0;
  portDISABLE_INTERRUPTS();
	for(int i = 0 ; i < 24 ; ++i){
		gpio_set_level(hx->clockpin, 1);
    ets_delay_us(CLOCK_DELAY_US);
    *val <<= 1u;
    gpio_set_level(hx->clockpin, 0);
    ets_delay_us(CLOCK_DELAY_US);
    if(gpio_get_level(hx->dpin)){
      ++*val;
    }
	}
  if(hx711_setgain(hx, eGAIN_128)){
    ret = -1;
  }
	portENABLE_INTERRUPTS();
  // sign extend 24-bit value to 32 bits
  if(*val & 0x800000ul){
    *val |= (0xfful << 24u);
  }
  return ret;
}
