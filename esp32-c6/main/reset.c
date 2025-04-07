#include "dankdryer.h"
#include "reset.h"
#include "pins.h"
#include <esp_log.h>
#include <esp_timer.h>
#include <stdatomic.h>
#include <driver/gpio.h>

#define TAG "freset"

#define FRESET_HOLD_TIME_US 5000000ul

// false means we last saw it low, true high
static bool FResetState;
static gpio_num_t FResetPin;
static _Atomic(int64_t) last_low_start = -1;

static void
freset_intr(void* v){
  ESP_LOGI(TAG, "got an interrupt");
  int s = gpio_get_level(FResetPin);
  if(s == FResetState && last_low_start >= 0){
    return; // no need to change
  }
  if(!s){ // either first sample, or new low sample
    last_low_start = esp_timer_get_time();
  }else{
    last_low_start = -1;
  }
  FResetState = s;
}

int setup_factory_reset(gpio_num_t pin){
  if(gpio_set_input(pin)){
    return -1;
  }
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
  FResetPin = pin;
  if((e = gpio_intr_enable(pin)) != ESP_OK){
    fprintf(stderr, "failure (%s) enabling %d interrupt\n", esp_err_to_name(e), pin);
    return -1;
  }
  return 0;
}

bool check_factory_reset(int64_t curtime){
  int64_t lls = last_low_start;
  if(lls < 0 || curtime < 0){
    return false;
  }
  if(gpio_get_level(FResetPin)){
    return false;
  }
  if(curtime - lls < FRESET_HOLD_TIME_US){
    return false;
  }
  return true;
}
