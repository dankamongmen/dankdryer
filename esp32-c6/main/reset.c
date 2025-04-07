#include "dankdryer.h"
#include "reset.h"
#include <esp_log.h>
#include <stdatomic.h>
#include <driver/gpio.h>

#define TAG "freset"

#define FRESET_HOLD_TIME_US 5000000ul
static _Atomic(int64_t) last_low_start = -1;

static void
freset_intr(void* v){
  ESP_LOGI(TAG, "got an interrupt");
}

int setup_factory_reset(gpio_num_t pin){
  esp_err_t e;
  if((e = gpio_pullup_dis(pin)) != ESP_OK){
    fprintf(stderr, "error (%s) disabling pullup on %d\n", esp_err_to_name(e), pin);
    return -1;
  }
  if((e = gpio_pulldown_en(pin)) != ESP_OK){
    fprintf(stderr, "error (%s) enabling pulldown on %d\n", esp_err_to_name(e), pin);
    return -1;
  }
  if((e = gpio_set_intr_type(pin, GPIO_INTR_ANYEDGE)) != ESP_OK){
    fprintf(stderr, "failure (%s) installing %d interrupt\n", esp_err_to_name(e), pin);
    return -1;
  }
  if((e = gpio_isr_handler_add(pin, freset_intr, NULL)) != ESP_OK){
    fprintf(stderr, "failure (%s) setting %d isr\n", esp_err_to_name(e), pin);
    return -1;
  }
  if((e = gpio_intr_enable(pin)) != ESP_OK){
    fprintf(stderr, "failure (%s) enabling %d interrupt\n", esp_err_to_name(e), pin);
    return -1;
  }
  return 0;
  return 0;
}

bool check_factory_reset(int64_t curtime){
  int64_t lls = last_low_start;
  if(lls < 0 || curtime < 0){
    return false;
  }
  if(curtime - lls < FRESET_HOLD_TIME_US){
    return false;
  }
  return true;
}
