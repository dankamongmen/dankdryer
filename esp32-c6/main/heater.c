#include "dankdryer.h"
#include "heater.h"
#include "pins.h"
#include <esp_log.h>
#include <soc/adc_channel.h>
#include <esp_adc/adc_oneshot.h>
#include <driver/temperature_sensor.h>

#define TAG "therm"

static bool HeaterState;
static float LastUpperTemp;
static bool ADC1Calibrated;
static adc_channel_t Thermchan;
static adc_oneshot_unit_handle_t ADC1;
static adc_cali_handle_t ADC1Calibration;

// initialize and calibrate an ADC unit (ESP32-S3 has two, but ADC2 is
// used by wifi). ADC1 supports GPIO 0--6.
static int setup_adc_oneshot(adc_unit_t unit, adc_oneshot_unit_handle_t* handle,
                             adc_cali_handle_t* cali, bool* calibrated){
  *calibrated = false;
  adc_oneshot_unit_init_cfg_t acfg = {
    .unit_id = unit,
    .ulp_mode = ADC_ULP_MODE_DISABLE,
  };
  esp_err_t e;
  if((e = adc_oneshot_new_unit(&acfg, handle)) != ESP_OK){
    ESP_LOGE(TAG, "error (%s) getting adc unit", esp_err_to_name(e));
    return -1;
  }
  // the ADC is designed around a 1100mV maximum input value. the LMT87 send
  // a value between 3277 mV (-50C) and 538 mV (150C). to handle such values,
  // we need attenuate the input signal. 12dB gives us up to 2450 mV, the
  // furthest we can go. to get the full range, we'd need a voltage divider
  // (something like 1000 + 470 ought work well). we don't really care about
  // such low values, so 12dB it is.
  adc_oneshot_chan_cfg_t cconf = {
    .bitwidth = ADC_BITWIDTH_DEFAULT,
    .atten = ADC_ATTEN_DB_12,
  };
  if((e = adc_oneshot_config_channel(*handle, Thermchan, &cconf)) != ESP_OK){
    ESP_LOGE(TAG, "error (%s) configuring adc channel", esp_err_to_name(e));
    adc_oneshot_del_unit(*handle);
    return -1;
  }
  adc_cali_curve_fitting_config_t caliconf = {
    .bitwidth = cconf.bitwidth,
    .atten = cconf.atten,
    .unit_id = unit,
    .chan = Thermchan,
  };
  if((e = adc_cali_create_scheme_curve_fitting(&caliconf, cali)) != ESP_OK){
    ESP_LOGE(TAG, "error (%s) creating adc calibration", esp_err_to_name(e));
    // go ahead and use the (uncalibrated) ADC if we must
  }else{
    ESP_LOGI(TAG, "using curve fitting adc calibration");
    *calibrated = true;
  }
  return 0;
}

// the esp32-s3 has a built in temperature sensor, which we enable.
// we furthermore set up the LM35 pin for input/ADC.
int setup_temp(gpio_num_t thermpin, adc_unit_t unit){
  if(gpio_set_input(thermpin)){
    return -1;
  }
  esp_err_t e;
  if((e = gpio_pullup_dis(thermpin)) != ESP_OK){
    ESP_LOGE(TAG, "error (%s) disabling pullup on %d", esp_err_to_name(e), thermpin);
    return -1;
  }
  if((e = adc_oneshot_io_to_channel(thermpin, &unit, &Thermchan)) != ESP_OK){
    ESP_LOGE(TAG, "error (%s) getting adc channel for %d", esp_err_to_name(e), thermpin);
    return -1;
  }
  if(setup_adc_oneshot(ADC_UNIT_1, &ADC1, &ADC1Calibration, &ADC1Calibrated)){
    return -1;
  }
  return 0;
}

bool get_heater_state(void){
  return HeaterState;
}

void set_heater(gpio_num_t pin, bool enabled){
  HeaterState = enabled;
  gpio_level(pin, enabled);
  ESP_LOGI(TAG, "set heater %s", bool_as_onoff(HeaterState));
}

static float
getLM35(adc_channel_t channel){
  esp_err_t e;
  int raw;
  float o;
  // FIXME if we didn't get an ADC handle at all, need to elide read entirely
  if(ADC1Calibrated){
    if((e = adc_oneshot_get_calibrated_result(ADC1, ADC1Calibration, channel, &raw)) != ESP_OK){
      ESP_LOGE(TAG, "error (%s) reading calibrated adc value %d", esp_err_to_name(e), raw);
      return MIN_TEMP - 1;
    }
    o = raw;
  }else{
    if((e = adc_oneshot_read(ADC1, channel, &raw)) != ESP_OK){
      ESP_LOGE(TAG, "error (%s) reading from adc", esp_err_to_name(e));
      return MIN_TEMP - 1;
    }
    // Dmax is 4095 on single read mode, 8191 on continuous
    // Vmax is 3100mA with ADC_ATTEN_DB_12, 1750 with _6, 1250 w/ _2_5
    // result is read * Vmax / Dmax
    o = raw * 1750.0 / 4095;
  }
  float ret = o / 10.0; // 10 mV per C
  ESP_LOGI(TAG, "%d -> %f -> %f", raw, o, ret);
  return ret;
}

// manage the heater based on the temperature of the hot chamber, which is
// sampled within this function.
float manage_heater(gpio_num_t ssrpin, time_t dryends, uint32_t targtemp){
  // if there is no drying scheduled, the heater ought be off, independent
  // of all other considerations.
  if(get_heater_state() && !dryends){
    set_heater(ssrpin, false);
  }
  float utemp = getLM35(Thermchan);
  if(!temp_valid_p(utemp)){
    // without a valid upper chamber measurement, it's unsafe to run the heater
    set_heater(ssrpin, false);
  }else{
    // take actions based on a valid read. even if we did just disable the
    // heater, we still want to update LastUpperTemp.
    LastUpperTemp = utemp;
    if(get_heater_state()){
      // turn it off if we're above the target temp FIXME do this better
      if(utemp >= targtemp){
        set_heater(ssrpin, false);
      }
      // turn it on if we're below the target temp, and scheduled to dry
    }else if(dryends && utemp < targtemp){
      set_heater(ssrpin, true);
    }
  }
  return utemp;
}

float get_upper_temp(void){
  return LastUpperTemp;
}
