#include "dankdryer.h"
#include "fans.h"
#include "pins.h"
#include <nvs.h>
#include <string.h>
#include <esp_log.h>
#include <driver/ledc.h>
#include <driver/gpio.h>

#define TAG "fans"

#define FANPWM_BIT_NUM LEDC_TIMER_8_BIT
#define RPMMAX (1u << 14u)
#define LOWER_FANCHAN 0
#define UPPER_FANCHAN 1
#define LOWER_FANTIMER LEDC_TIMER_0
#define UPPER_FANTIMER LEDC_TIMER_1

// variables manipulated by interrupts
static _Atomic(uint32_t) LowerFanPulses, UpperFanPulses;

static uint32_t LowerPWM = 128;
static uint32_t UpperPWM = 128;

static inline bool
pwm_valid_p(int pwm){
  return pwm >= 0 && pwm <= MAXPWMDUTY;
}

static int
set_pwm(const ledc_channel_t channel, unsigned pwm){
  if(ledc_set_duty_and_update(LEDCMODE, channel, pwm, MAXPWMDUTY) != ESP_OK){
    ESP_LOGE(TAG, "error setting pwm!");
    return -1;
  }
  printf("set pwm to %u on channel %d\n", pwm, channel);
  return 0;
}

static int
write_pwm(const char* recname, uint32_t pwm){
  nvs_handle_t nvsh;
  esp_err_t err = nvs_open(NVS_HANDLE_NAME, NVS_READWRITE, &nvsh);
  if(err){
    ESP_LOGE(TAG, "error (%s) opening nvs:" NVS_HANDLE_NAME, esp_err_to_name(err));
    return -1;
  }
  err = nvs_set_u32(nvsh, recname, pwm);
  if(err){
    ESP_LOGE(TAG, "error (%s) writing " NVS_HANDLE_NAME ":%s", esp_err_to_name(err), recname);
    nvs_close(nvsh);
    return -1;
  }
  err = nvs_commit(nvsh);
  if(err){
    ESP_LOGE(TAG, "error (%s) committing nvs:" NVS_HANDLE_NAME, esp_err_to_name(err));
    nvs_close(nvsh);
    return -1;
  }
  nvs_close(nvsh);
  return 0;
}

void set_lower_pwm(unsigned pwm){
  LowerPWM = pwm;
  write_pwm(LOWERPWM_RECNAME, pwm);
  set_pwm(LOWER_FANCHAN, LowerPWM);
}

void set_upper_pwm(unsigned pwm){
  UpperPWM = pwm;
  write_pwm(UPPERPWM_RECNAME, pwm);
  set_pwm(UPPER_FANCHAN, UpperPWM);
}

unsigned get_lower_pwm(void){
  return LowerPWM;
}

unsigned get_upper_pwm(void){
  return UpperPWM;
}

int read_fans_pstore(nvs_handle_t nvsh){
  uint32_t lpwm = get_lower_pwm();
  if(nvs_get_opt_u32(nvsh, LOWERPWM_RECNAME, &lpwm) == 0){
    if(pwm_valid_p(lpwm)){
      LowerPWM = lpwm;
    }else{
      ESP_LOGE(TAG, "read invalid lower pwm %lu", lpwm);
    }
  }
  uint32_t upwm = get_upper_pwm();
  if(nvs_get_opt_u32(nvsh, UPPERPWM_RECNAME, &upwm) == 0){
    if(pwm_valid_p(upwm)){
      UpperPWM = upwm;
    }else{
      ESP_LOGE(TAG, "read invalid upper pwm %lu", upwm);
    }
  }
  return 0;
}

static int
initialize_pwm(ledc_channel_t channel, gpio_num_t pin, int freq, ledc_timer_t timer){
  if(gpio_set_output(pin)){
    return -1;
  }
  ledc_timer_config_t ledc_timer;
  memset(&ledc_timer, 0, sizeof(ledc_timer));
  ledc_timer.speed_mode = LEDCMODE;
  ledc_timer.duty_resolution = FANPWM_BIT_NUM;
  ledc_timer.timer_num = timer;
  ledc_timer.freq_hz = freq;
  if(ledc_timer_config(&ledc_timer) != ESP_OK){
    ESP_LOGE(TAG, "error (timer config)!\n");
    return -1;
  }
  ledc_channel_config_t conf;
  memset(&conf, 0, sizeof(conf));
  conf.gpio_num = pin;
  conf.speed_mode = LEDCMODE;
  conf.intr_type = LEDC_INTR_DISABLE;
  conf.timer_sel = timer;
  conf.duty = FANPWM_BIT_NUM;
  conf.channel = channel;
  printf("setting up pin %d for %dHz PWM\n", pin, freq);
  if(ledc_channel_config(&conf) != ESP_OK){
    ESP_LOGE(TAG, "error (channel config)!\n");
    return -1;
  }
  return 0;
}

static int
initialize_25k_pwm(ledc_channel_t channel, gpio_num_t pin, ledc_timer_t timer){
  return initialize_pwm(channel, pin, 25000, timer);
}

int setup_fans(gpio_num_t lowerppin, gpio_num_t upperppin,
               gpio_num_t lowertpin, gpio_num_t uppertpin){
  int ret = 0;
  esp_err_t e;
  if((e = ledc_fade_func_install(0)) != ESP_OK){
    ESP_LOGE(TAG, "error (%s) installing ledc interrupt", esp_err_to_name(e));
    ret = -1;
  }
  if(setup_intr(lowertpin, &LowerFanPulses)){
    ret = -1;
  }
  if(initialize_25k_pwm(LOWER_FANCHAN, lowerppin, LOWER_FANTIMER)
      || set_pwm(LOWER_FANCHAN, LowerPWM)){
    ret = -1;
  }
  if(setup_intr(uppertpin, &UpperFanPulses)){
    ret = -1;
  }
  if(initialize_25k_pwm(UPPER_FANCHAN, upperppin, UPPER_FANTIMER)
      || set_pwm(UPPER_FANCHAN, UpperPWM)){
    ret = -1;
  }
  return ret;
}

uint32_t get_lower_tach(void){
  uint32_t r = LowerFanPulses;
  LowerFanPulses = 0;
  return r;
}

uint32_t get_upper_tach(void){
  uint32_t r = UpperFanPulses;
  UpperFanPulses = 0;
  return r;
}
