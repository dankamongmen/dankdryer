// intended for use on an ESP32-S3-WROOM-1
#include "networking.h"
#include "emc230x.h"
#include "nau7802.h"
#include "pins.h"
#include "ota.h"
#include <nvs.h>
#include <time.h>
#include <math.h>
#include <ctype.h>
#include <cJSON.h>
#include <string.h>
#include <led_strip.h>
#include <nvs_flash.h>
#include <esp_timer.h>
#include <esp_system.h>
#include <driver/ledc.h>
#include <esp_app_desc.h>
#include <hal/ledc_types.h>
#include <esp_idf_version.h>
#include <soc/adc_channel.h>
#include <esp_adc/adc_cali.h>
#include <driver/i2c_master.h>
#include <esp_adc/adc_oneshot.h>
#include <driver/temperature_sensor.h>

#define UUIDLEN 16
#define FANPWM_BIT_NUM LEDC_TIMER_8_BIT
#define RPMMAX (1u << 14u)
#define MIN_TEMP -80
#define MAX_TEMP 200
#define MAX_DRYREQ_TMP 150
#define MIN_DRYREQ_TMP 50
// minimum of 15s between mqtt publications
#define MQTT_PUBLISH_QUANTUM_USEC 15000000ul
// give tachs a longer period to smooth them out
#define TACH_SAMPLE_QUANTUM_USEC 5000000ul

#define NVS_HANDLE_NAME "pstore"
#define BOOTCOUNT_RECNAME "bootcount"
#define TAREOFFSET_RECNAME "tare"
#define LOWERPWM_RECNAME "lpwm"
#define UPPERPWM_RECNAME "upwm"
#define ESSID_RECNAME "essid"
#define PSK_RECNAME "psk"
#define SETUPSTATE_RECNAME "sstate"

#define LOAD_CELL_MAX 5000 // 5kg capable

static const unsigned LOWER_FANCHAN = 0;
static const unsigned UPPER_FANCHAN = 1;

static bool MotorState;
static bool HeaterState;
static uint32_t LowerPWM = 128;
static uint32_t UpperPWM = 128;
static float LastWeight = -1.0;
static float TareWeight = -1.0;
static adc_channel_t Thermchan;
static time_t DryEndsAt; // dry stop time in seconds since epoch
static uint32_t TargetTemp; // valid iff DryEndsAt != 0
static unsigned LastLowerRPM, LastUpperRPM;
static float LastLowerTemp, LastUpperTemp;
static i2c_master_bus_handle_t I2CMaster;
static i2c_master_dev_handle_t NAU7802;
static i2c_master_dev_handle_t EMC2302;

// ESP-IDF objects
static bool ADC1Calibrated;
static led_strip_handle_t Neopixel;
static adc_oneshot_unit_handle_t ADC1;
static temperature_sensor_handle_t temp;
static adc_cali_handle_t ADC1Calibration;

static inline bool
rpm_valid_p(unsigned rpm){
  return rpm < 3000;
}

static inline bool
pwm_valid_p(int pwm){
  return pwm >= 0 && pwm <= 255;
}

static inline bool
temp_valid_p(float temp){
  return temp >= MIN_TEMP && temp <= MAX_TEMP;
}

static inline bool
weight_valid_p(float weight){
  return weight >= 0 && weight <= LOAD_CELL_MAX;
}

typedef struct failure_indication {
  int r, g, b;
} failure_indication;

static bool StartupFailure;
static const failure_indication SystemError = { 32, 0, 0 };
static const failure_indication NetworkError = { 32, 0, 32 };
static const failure_indication PostFailure = { 0, 0, 0 };

// precondition: isxdigit(c) is true
static inline char
get_hex(char c){
  if(isdigit(c)){
    return c - '0';
  }
  c = tolower(c);
  return c - 'a' + 10;
}

float get_tare(void){
  return TareWeight;
}

float get_upper_temp(void){
  return LastUpperTemp;
}

float get_lower_temp(void){
  return LastLowerTemp;
}

unsigned long long get_dry_ends_at(void){
  return DryEndsAt;
}

unsigned long get_target_temp(void){
  return TargetTemp;
}

float get_weight(void){
  return LastWeight;
}

unsigned get_lower_pwm(void){
  return LowerPWM;
}

unsigned get_upper_pwm(void){
  return UpperPWM;
}

unsigned get_lower_rpm(void){
  return LastLowerRPM;
}

unsigned get_upper_rpm(void){
  return LastUpperRPM;
}

static int
write_pwm(const char* recname, uint32_t pwm){
  nvs_handle_t nvsh;
  esp_err_t err = nvs_open(NVS_HANDLE_NAME, NVS_READWRITE, &nvsh);
  if(err){
    fprintf(stderr, "error (%s) opening nvs:" NVS_HANDLE_NAME "\n", esp_err_to_name(err));
    return -1;
  }
  err = nvs_set_u32(nvsh, recname, pwm);
  if(err){
    fprintf(stderr, "error (%s) writing " NVS_HANDLE_NAME ":%s\n", esp_err_to_name(err), recname);
    nvs_close(nvsh);
    return -1;
  }
  err = nvs_commit(nvsh);
  if(err){
    fprintf(stderr, "error (%s) committing nvs:" NVS_HANDLE_NAME "\n", esp_err_to_name(err));
    nvs_close(nvsh);
    return -1;
  }
  nvs_close(nvsh);
  return 0;
}

int read_wifi_config(unsigned char* essid, size_t essidlen,
                     unsigned char* psk, size_t psklen,
                     int* setupstate){
  nvs_handle_t nvsh;
  esp_err_t err = nvs_open(NVS_HANDLE_NAME, NVS_READWRITE, &nvsh);
  if(err){
    fprintf(stderr, "failure (%d) opening nvs:" NVS_HANDLE_NAME "\n", err);
    return -1;
  }
  if(nvs_get_str(nvsh, ESSID_RECNAME, (char*)essid, &essidlen) != ESP_OK){
    fprintf(stderr, "failure (%s) reading essid\n", esp_err_to_name(err));
    goto err;
  }
  if(nvs_get_str(nvsh, PSK_RECNAME, (char*)psk, &psklen) != ESP_OK){
    fprintf(stderr, "failure (%s) reading psk\n", esp_err_to_name(err));
    goto err;
  }
  printf("read configured %zuB essid [%s] %zuB psk\n", essidlen - 1, essid, psklen - 1);
  if(psklen <= 1 || essidlen <= 1){
    goto err;
  }
  uint32_t rawstate = 0;
  if(nvs_get_opt_u32(nvsh, SETUPSTATE_RECNAME, &rawstate) == 0){
    if(rawstate <= 2 && rawstate > 0){ // FIXME export semantics or call to check it
      *setupstate = rawstate;
      printf("read setup state %d, applied\n", *setupstate);
    }else{
      fprintf(stderr, "read invalid setup state %lu\n", rawstate);
      goto err;
    }
  }
  return 0;

err:
  nvs_close(nvsh);
  memset(essid, 0, essidlen);
  memset(psk, 0, psklen);
  return -1;
}

int write_wifi_config(const unsigned char* essid, const unsigned char* psk,
                      uint32_t state){
  nvs_handle_t nvsh;
  esp_err_t err = nvs_open(NVS_HANDLE_NAME, NVS_READWRITE, &nvsh);
  if(err){
    fprintf(stderr, "error (%s) opening nvs:" NVS_HANDLE_NAME "\n", esp_err_to_name(err));
    return -1;
  }
  err = nvs_set_str(nvsh, ESSID_RECNAME, (const char*)essid);
  if(err){
    fprintf(stderr, "error (%s) writing " NVS_HANDLE_NAME ":%s\n", esp_err_to_name(err), ESSID_RECNAME);
    nvs_close(nvsh);
    return -1;
  }
  err = nvs_set_str(nvsh, PSK_RECNAME, (const char*)psk);
  if(err){
    fprintf(stderr, "error (%s) writing " NVS_HANDLE_NAME ":%s\n", esp_err_to_name(err), PSK_RECNAME);
    nvs_close(nvsh);
    return -1;
  }
  err = nvs_set_u32(nvsh, SETUPSTATE_RECNAME, state);
  if(err){
    fprintf(stderr, "error (%s) writing " NVS_HANDLE_NAME ":%s\n", esp_err_to_name(err), SETUPSTATE_RECNAME);
    nvs_close(nvsh);
    return -1;
  }
  err = nvs_commit(nvsh);
  if(err){
    fprintf(stderr, "error (%s) committing nvs:" NVS_HANDLE_NAME "\n", esp_err_to_name(err));
    nvs_close(nvsh);
    return -1;
  }
  nvs_close(nvsh);
  return 0;
}

static int
set_pwm(unsigned fanidx, unsigned pwm){
  if(emc230x_setpwm(EMC2302, fanidx, pwm)){
    fprintf(stderr, "error setting pwm %u for fan %u!\n", pwm, fanidx);
    return -1;
  }
  printf("set pwm to %u on fan %d\n", pwm, fanidx);
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

// update NVS with tare offset
static int
write_tare_offset(float tare){
  nvs_handle_t nvsh;
  esp_err_t err = nvs_open(NVS_HANDLE_NAME, NVS_READWRITE, &nvsh);
  if(err){
    fprintf(stderr, "error (%s) opening nvs:" NVS_HANDLE_NAME "\n", esp_err_to_name(err));
    return -1;
  }
  char buf[20];
  if(snprintf(buf, sizeof(buf), "%.8f", tare) > sizeof(buf)){
    fprintf(stderr, "warning: couldn't store tare offset %f in buf\n", tare);
  }
  err = nvs_set_str(nvsh, TAREOFFSET_RECNAME, buf);
  if(err){
    fprintf(stderr, "error (%s) writing " NVS_HANDLE_NAME ":" TAREOFFSET_RECNAME "\n", esp_err_to_name(err));
    nvs_close(nvsh);
    return -1;
  }
  err = nvs_commit(nvsh);
  if(err){
    fprintf(stderr, "error (%s) committing nvs:" NVS_HANDLE_NAME "\n", esp_err_to_name(err));
    nvs_close(nvsh);
    return -1;
  }
  nvs_close(nvsh);
  return 0;
}

void set_tare(void){
  if(weight_valid_p(LastWeight)){
    TareWeight = LastWeight;
    write_tare_offset(TareWeight);
    printf("tared at %f\n", TareWeight);
  }else{
    fprintf(stderr, "requested tare, but no valid measurements yet\n");
  }
}

// FIXME ignore whitespace
// check if data (dlen bytes, no terminator) equals n, returning true
// if it does, and false otherwise.
static bool
strarg_match_p(const char* data, size_t dlen, const char* n){
  if(dlen != strlen(n)){
    return false;
  }
  return strncasecmp(data, n, dlen) ? false : true;
}

int extract_bool(const char* data, size_t dlen, bool* val){
  if(strarg_match_p(data, dlen, "on") || strarg_match_p(data, dlen, "yes") ||
      strarg_match_p(data, dlen, "true") || strarg_match_p(data, dlen, "1")){
    *val = true;
    return 0;
  }
  if(strarg_match_p(data, dlen, "off") || strarg_match_p(data, dlen, "no") ||
      strarg_match_p(data, dlen, "false") || strarg_match_p(data, dlen, "0")){
    *val = false;
    return 0;
  }
  printf("not a bool: [%s]\n", data);
  return -1;
}

// FIXME handle base 10 numbers as well (can we use strtoul?)
int extract_pwm(const char* data, size_t dlen){
  if(dlen != 2){
    printf("pwm wasn't 2 characters\n");
    return -1;
  }
  char h = data[0];
  char l = data[1];
  if(!isxdigit(h) || !isxdigit(l)){
    printf("invalid hex character\n");
    return -1;
  }
  char hb = get_hex(h);
  char lb = get_hex(l);
  // everything was valid
  int pwm = hb * 16 + lb;
  printf("got pwm value: %d\n", pwm);
  return pwm;
}

// on error, returns MIN_TEMP - 1
static float
getAmbient(void){
  float t;
  if(temperature_sensor_get_celsius(temp, &t)){
    fprintf(stderr, "failed acquiring temperature\n");
    return MIN_TEMP - 1;
  }
  return t;
}

float getWeight(void){
  int32_t v;
  if(nau7802_read(NAU7802, &v) || v < 0){
    fprintf(stderr, "bad nau7802 read %ld\n", v);
    return -1.0;
  }
  float tare = 0;
  if(weight_valid_p(TareWeight)){
    tare = TareWeight;
  }
  float newv = (float)v * LOAD_CELL_MAX / 0xfffffful;
  printf("scaling ADC of %ld to %f, tare (%f) to %f\n", v, newv, tare, newv - tare);
  newv -= tare;
  return newv;
}

// gpio_reset_pin() disables input and output, selects for GPIO, and enables pullup
static int
gpio_setup(gpio_num_t pin, gpio_mode_t mode, const char *mstr){
  gpio_reset_pin(pin);
  esp_err_t err;
  if((err = gpio_set_direction(pin, mode)) != ESP_OK){
    fprintf(stderr, "failure (%s) setting %d to %s\n", esp_err_to_name(err), pin, mstr);
    return -1;
  }
  return 0;
}

static inline int
gpio_set_inputoutput_opendrain(gpio_num_t pin){
  return gpio_setup(pin, GPIO_MODE_INPUT_OUTPUT_OD, "input+output(od)");
}

static inline int
gpio_set_inputoutput(gpio_num_t pin){
  return gpio_setup(pin, GPIO_MODE_INPUT_OUTPUT, "input+output");
}

static inline int
gpio_set_input(gpio_num_t pin){
  return gpio_setup(pin, GPIO_MODE_INPUT, "input");
}

static inline int
gpio_set_output(gpio_num_t pin){
  return gpio_setup(pin, GPIO_MODE_OUTPUT, "output");
}

static int
rgbset(unsigned r, unsigned g, unsigned b){
  esp_err_t e = led_strip_set_pixel(Neopixel, 0, r, g, b);
  if(e != ESP_OK){
    fprintf(stderr, "failure %d setting LED\n", e);
    return -1;
  }
  if((e = led_strip_refresh(Neopixel)) != ESP_OK){
    fprintf(stderr, "failure %d refreshing LED\n", e);
    return -1;
  }
  return 0;
}

#ifdef RGB_PIN
static int
setup_neopixel(gpio_num_t pin){
  if(gpio_set_output(pin)){
    return -1;
  }
  led_strip_config_t sconf = {
    .strip_gpio_num = pin,
    .max_leds = 1,
    .led_model = LED_MODEL_WS2812,
    .flags.invert_out = false,
  };
  led_strip_rmt_config_t rconf = {
    .clk_src = RMT_CLK_SRC_DEFAULT,
    .resolution_hz = 10 * 1000 * 1000,
    .flags.with_dma = false,
  };
  esp_err_t e;
  if((e = led_strip_new_rmt_device(&sconf, &rconf, &Neopixel)) != ESP_OK){
    fprintf(stderr, "failure %d initializing LED at %d\n", e, pin);
    return -1;
  }
  if(rgbset(64, 64, 64)){
    return -1;
  }
  return 0;
}
#endif

// initialize and calibrate an ADC unit (ESP32-S3 has two, but ADC2 is
// used by wifi). ADC1 supports 8 channels on pins 32--39.
static int
setup_adc_oneshot(adc_unit_t unit, adc_oneshot_unit_handle_t* handle,
                  adc_cali_handle_t* cali, bool* calibrated){
  esp_err_t e;
  adc_oneshot_unit_init_cfg_t acfg = {
    .unit_id = unit,
    .ulp_mode = ADC_ULP_MODE_DISABLE,
  };
  if((e = adc_oneshot_new_unit(&acfg, handle)) != ESP_OK){
    fprintf(stderr, "error (%s) getting adc unit\n", esp_err_to_name(e));
    return -1;
  }
  adc_cali_curve_fitting_config_t caliconf = {
    .bitwidth = ADC_BITWIDTH_DEFAULT,
    .atten = ADC_ATTEN_DB_2_5,
    .unit_id = unit,
  };
  if((e = adc_cali_create_scheme_curve_fitting(&caliconf, cali)) != ESP_OK){
    fprintf(stderr, "error (%s) creating adc calibration\n", esp_err_to_name(e));
    *calibrated = false;
  }else{
    printf("using curve fitting adc calibration\n");
    *calibrated = true;
  }
  return 0;
}

// the esp32-s3 has a built in temperature sensor, which we enable.
// we furthermore set up the LM35 pin for input/ADC.
static int
setup_temp(gpio_num_t thermpin, adc_unit_t unit, adc_channel_t* channel){
  temperature_sensor_config_t conf = TEMPERATURE_SENSOR_CONFIG_DEFAULT(-10, 80);
  if(temperature_sensor_install(&conf, &temp) != ESP_OK){
    fprintf(stderr, "failed to set up thermostat\n");
    return -1;
  }
  if(temperature_sensor_enable(temp) != ESP_OK){
    fprintf(stderr, "failed to enable thermostat\n");
    return -1;
  }
  if(gpio_set_input(thermpin)){
    return -1;
  }
  esp_err_t e;
  if((e = adc_oneshot_io_to_channel(thermpin, &unit, channel)) != ESP_OK){
    fprintf(stderr, "error (%s) getting adc channel for %d\n", esp_err_to_name(e), thermpin);
    return -1;
  }
  return 0;
}

static int
init_pstore(void){
  printf("initializing persistent store...\n");
  esp_err_t err = nvs_flash_init();
  if(err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND){
    // FIXME LED feedback during erase + init
    printf("erasing flash...\n");
    err = nvs_flash_erase();
    if(err == ESP_OK){
      printf("initializing flash...\n");
      err = nvs_flash_init();
    }
  }
  if(err){
    // FIXME LED feedback
    fprintf(stderr, "failure (%d) initializing nvs!\n", err);
    return -1;
  }
  return 0;
}

// check for an optional record in the nvs handle. if not defined, return 0.
// if defined, load val and return 0. on other errors, return -1.
int nvs_get_opt_u32(nvs_handle_t nh, const char* recname, uint32_t* val){
  esp_err_t err = nvs_get_u32(nh, recname, val);
  if(err == ESP_ERR_NVS_NOT_FOUND){
    printf("no record '%s' in nvs\n", recname);
    return 0;
  }else if(err){
    fprintf(stderr, "failure (%d) reading %s\n", err, recname);
    return -1;
  }
  printf("read configured default %lu from nvs:%s\n", *val, recname);
  return 0;
}

// NVS can't use floats directly. we instead write/read them as strings.
static int
nvs_get_opt_float(nvs_handle_t nh, const char* recname, float* val){
  char buf[32];
  unsigned blen = sizeof(buf);
  esp_err_t err = nvs_get_str(nh, recname, buf, &blen);
  if(err == ESP_ERR_NVS_NOT_FOUND){
    printf("no record '%s' in nvs\n", recname);
    return -1;
  }else if(err){
    fprintf(stderr, "failure (%s) reading %s\n", esp_err_to_name(err), recname);
    return -1;
  }
  char* bend;
  *val = strtof(buf, &bend);
  if(bend || !*val || isnan(*val)){
    fprintf(stderr, "couldn't convert [%s] to float for nvs:%s\n", buf, recname);
    return -1;
  }
  printf("read configured default %f from nvs:%s\n", *val, recname);
  return 0;
}

// read and update boot count, read configurable defaults from pstore if they
// are present (we do not write defaults to pstore, so we can differentiate
// between defaults and a configured value).
static int
read_pstore(void){
  nvs_handle_t nvsh;
  esp_err_t err = nvs_open(NVS_HANDLE_NAME, NVS_READWRITE, &nvsh);
  if(err){
    fprintf(stderr, "failure (%d) opening nvs:" NVS_HANDLE_NAME "\n", err);
    return -1;
  }
  uint32_t bootcount = 0;
  err = nvs_get_u32(nvsh, BOOTCOUNT_RECNAME, &bootcount);
  if(err && err != ESP_ERR_NVS_NOT_FOUND){
    fprintf(stderr, "failure (%d) reading " NVS_HANDLE_NAME ":" BOOTCOUNT_RECNAME "\n", err);
    nvs_close(nvsh);
    return -1;
  }
  ++bootcount;
  printf("this is boot #%lu\n", bootcount);
  err = nvs_set_u32(nvsh, BOOTCOUNT_RECNAME, bootcount);
  if(err){
    fprintf(stderr, "failure (%d) writing " NVS_HANDLE_NAME ":" BOOTCOUNT_RECNAME "\n", err);
    nvs_close(nvsh);
    return -1;
  }
  err = nvs_commit(nvsh);
  if(err){
    fprintf(stderr, "failure (%d) committing nvs:" NVS_HANDLE_NAME "\n", err);
    nvs_close(nvsh);
    return -1;
  }
  uint32_t lpwm = LowerPWM;
  if(nvs_get_opt_u32(nvsh, LOWERPWM_RECNAME, &lpwm) == 0){
    if(pwm_valid_p(lpwm)){
      LowerPWM = lpwm;
    }else{
      fprintf(stderr, "read invalid lower pwm %lu\n", lpwm);
    }
  }
  uint32_t upwm = UpperPWM;
  if(nvs_get_opt_u32(nvsh, UPPERPWM_RECNAME, &upwm) == 0){
    if(pwm_valid_p(upwm)){
      UpperPWM = upwm;
    }else{
      fprintf(stderr, "read invalid upper pwm %lu\n", upwm);
    }
  }
  float tare = TareWeight; // if not present, don't change initialized value
  if(nvs_get_opt_float(nvsh, TAREOFFSET_RECNAME, &tare) == 0){
    if(weight_valid_p(tare)){
      TareWeight = tare;
    }else{
      fprintf(stderr, "read invalid tare offset %f\n", tare);
    }
  }
  nvs_close(nvsh);
  return 0;
}

static int
setup_emc2302(i2c_master_bus_handle_t master){
  if(emc230x_detect(master, &EMC2302, EMC2302_MODEL_UNSPEC)){
    return -1;
  }
  if(set_pwm(LOWER_FANCHAN, LowerPWM)){
    return -1;
  }
  if(set_pwm(UPPER_FANCHAN, UpperPWM)){
    return -1;
  }
  return 0;
}

static int
setup_nau7802(i2c_master_bus_handle_t master){
  if(nau7802_detect(master, &NAU7802)){
    return -1;
  }
  if(nau7802_poweron(NAU7802)){
    return -1;
  }
  // FIXME set gain / LDO?
  return 0;
}

static void
set_led(const struct failure_indication *nin){
  rgbset(nin->r, nin->g, nin->b);
}

static void
set_failure(const struct failure_indication *fin){
  if(fin != &PostFailure){
    StartupFailure = true;
  }
  set_led(fin);
}

static int
gpio_level(gpio_num_t pin, bool level){
  esp_err_t e = gpio_set_level(pin, level);
  if(e != ESP_OK){
    fprintf(stderr, "error (%s) setting pin %d to %u\n", esp_err_to_name(e), pin, level);
    return -1;
  }
  return 0;
}

static inline const char*
bool_as_onoff(bool b){
  return b ? "on" : "off";
}

static inline const char*
motor_state(void){
  return bool_as_onoff(MotorState);
}

static inline const char*
heater_state(void){
  return bool_as_onoff(HeaterState);
}

bool get_motor_state(void){
  return MotorState;
}

bool get_heater_state(void){
  return HeaterState;
}

void set_motor(bool enabled){
  MotorState = enabled;
  gpio_level(MOTOR_GATEPIN, enabled);
  printf("set motor %s\n", motor_state());
}

void set_heater(bool enabled){
  HeaterState = enabled;
  gpio_level(SSR_GPIN, enabled);
  printf("set heater %s\n", heater_state());
}

static float
getLM35(adc_channel_t channel){
  esp_err_t e;
  int raw;
  e = adc_oneshot_read(ADC1, channel, &raw);
  if(e != ESP_OK){
    fprintf(stderr, "error (%s) reading from adc\n", esp_err_to_name(e));
    return MIN_TEMP - 1;
  }
  int vout;
  if(ADC1Calibrated){
    if((e = adc_cali_raw_to_voltage(ADC1Calibration, raw, &vout)) != ESP_OK){
      fprintf(stderr, "error (%s) calibrating raw adc value %d\n", esp_err_to_name(e), raw);
      vout = raw;
    }
  }
  // Dmax is 4095 on single read mode, 8191 on continuous
  // Vmax is 3100mA with ADC_ATTEN_DB_12, 1750 with _6, 1250 w/ _2_5
  float o = vout / 4095.0 * 125;
  printf("ADC1: converted %d to %d -> %f\n", raw, vout, o);
  return o;
}

// if the upper chamber temperature as measured is a valid sample, update
// LastUpperTemp, and turn heater on or off if appropriate.
static float
update_upper_temp(void){
  float utemp = getLM35(Thermchan);
  printf("lm35: %f\n", utemp);
  // if there is no drying scheduled, but the heater is on, we ought turn
  // it off even if we got an invalid temperature
  if(HeaterState && !DryEndsAt){
    set_heater(false);
  }
  // take actions based on a valid read. even if we did just disable the
  // heater, we still want to update LastUpperTemp.
  if(temp_valid_p(utemp)){
    LastUpperTemp = utemp;
    if(HeaterState){
      // turn it off if we're above the target temp
      if(utemp >= TargetTemp){
        set_heater(false);
      }
      // turn it on if we're below the target temp, and scheduled to dry
    }else if(DryEndsAt && utemp < TargetTemp){
      set_heater(true);
    }
  }
  return utemp;
}

int handle_dry(unsigned seconds, unsigned temp){
  printf("dry request for %us at %uC\n", seconds, temp);
  if(temp > MAX_DRYREQ_TMP || temp < MIN_DRYREQ_TMP){
    fprintf(stderr, "invalid temp request (%u)\n", temp);
    return -1;
  }
  DryEndsAt = esp_timer_get_time() + seconds * 1000000ull;
  set_motor(seconds != 0);
  TargetTemp = temp;
  update_upper_temp();
  return 0;
}

void factory_reset(void){
  printf("requested factory reset, erasing nvs...\n");
  set_motor(false);
  set_heater(false);
  esp_err_t e = nvs_flash_erase();
  if(e != ESP_OK){
    fprintf(stderr, "error (%s) erasing nvs\n", esp_err_to_name(e));
  }
  printf("rebooting\n");
  esp_restart();
}

static int
setup_motor(gpio_num_t mrelaypin){
  if(gpio_set_output(mrelaypin)){
    return -1;
  }
  set_motor(false);
  return 0;
}

static int
setup_heater(gpio_num_t hrelaypin){
  if(gpio_set_output(hrelaypin)){
    return -1;
  }
  set_heater(false);
  return 0;
}

static int
setup_i2c(i2c_master_bus_handle_t *master, gpio_num_t sda, gpio_num_t scl){
  i2c_master_bus_config_t i2ccnf = {
    .clk_source = I2C_CLK_SRC_DEFAULT,
    .i2c_port = -1,
    .scl_io_num = scl,
    .sda_io_num = sda,
    .glitch_ignore_cnt = 7, // recommended value from esp-idf docs
  };
  if(gpio_set_inputoutput_opendrain(sda) || gpio_set_inputoutput_opendrain(scl)){
    return -1;
  }
  esp_err_t e = i2c_new_master_bus(&i2ccnf, master);
  if(e != ESP_OK){
    fprintf(stderr, "failure (%s) initializing i2c master\n", esp_err_to_name(e));
    return -1;
  }
  return 0;
}

static void
setup(adc_channel_t* thermchan){
#ifdef RGB_PIN
  setup_neopixel(RGB_PIN);
#endif
  printf(DEVICE " v" VERSION "\n");
  if(!init_pstore()){
    read_pstore();
  }
  if(ota_init()){
    set_failure(&SystemError);
  }
  if(setup_adc_oneshot(ADC_UNIT_1, &ADC1, &ADC1Calibration, &ADC1Calibrated)){
    set_failure(&SystemError);
  }
  if(setup_i2c(&I2CMaster, SDA_PIN, SCL_PIN)){
    set_failure(&SystemError);
  }
  if(setup_temp(THERM_DATAPIN, ADC_UNIT_1, thermchan)){
    set_failure(&SystemError);
  }
  if(setup_emc2302(I2CMaster)){
    set_failure(&SystemError);
  }
  if(setup_nau7802(I2CMaster)){
    set_failure(&SystemError);
  }
  if(setup_heater(SSR_GPIN)){
    set_failure(&SystemError);
  }
  if(setup_motor(MOTOR_GATEPIN)){
    set_failure(&SystemError);
  }
  if(setup_network()){
    set_failure(&NetworkError);
  }
  //gpio_dump_all_io_configuration(stdout, SOC_GPIO_VALID_GPIO_MASK);
  if(!StartupFailure){
    set_failure(&PostFailure);
  }
  printf("initialization %ssuccessful v" VERSION "\n", StartupFailure ? "un" : "");
  //gpio_dump_io_configuration(stdout, SOC_GPIO_VALID_GPIO_MASK);
}

// we don't try to measure the first iteration, as we don't yet have a
// timestamp (and the fans are spinning up, anyway).
static void
getFanTachs(unsigned *lrpm, unsigned *urpm, int64_t curtime, int64_t lasttime){
  const float diffu = curtime - lasttime;
  *lrpm = *urpm = 0;
  emc230x_gettach(EMC2302, LOWER_FANCHAN, lrpm);
  emc230x_gettach(EMC2302, UPPER_FANCHAN, urpm);
  printf("tach raw: %u %u\n", *lrpm, *urpm);
  *lrpm /= 2; // two pulses for each rotation
  *urpm /= 2;
  const float scale = 60.0 * 1000000u / diffu;
  *lrpm *= scale;
  *urpm *= scale;
  printf("tachscale: %f diffu: %f lrpm: %u urpm: %u\n", scale, diffu, *lrpm, *urpm);
  if(rpm_valid_p(*lrpm)){
    LastLowerRPM = *lrpm;
  }
  if(rpm_valid_p(*urpm)){
    LastUpperRPM = *urpm;
  }
}

void send_mqtt(int64_t curtime, unsigned lrpm, unsigned urpm){
  cJSON* root = cJSON_CreateObject();
  if(root == NULL){
    fprintf(stderr, "couldn't create JSON object\n");
    return;
  }
  cJSON_AddNumberToObject(root, "uptimesec", curtime / 1000000ll);
  if(temp_valid_p(LastLowerTemp)){
    cJSON_AddNumberToObject(root, "dtemp", LastLowerTemp);
  }
  // UINT_MAX is sentinel for known bad reading, but anything over 3KRPM on
  // these Noctua NF-A8 fans is indicative of error; they max out at 2500.
  if(rpm_valid_p(LastLowerRPM)){
    cJSON_AddNumberToObject(root, "lrpm", LastLowerRPM);
  }
  if(rpm_valid_p(LastUpperRPM)){
    cJSON_AddNumberToObject(root, "urpm", LastUpperRPM);
  }
  cJSON_AddNumberToObject(root, "lpwm", LowerPWM);
  cJSON_AddNumberToObject(root, "upwm", UpperPWM);
  if(weight_valid_p(LastWeight)){
    cJSON_AddNumberToObject(root, "mass", LastWeight);
  }
  cJSON_AddNumberToObject(root, "tare", TareWeight);
  cJSON_AddNumberToObject(root, "motor", MotorState);
  cJSON_AddNumberToObject(root, "heater", HeaterState);
  if(temp_valid_p(LastUpperTemp)){
    cJSON_AddNumberToObject(root, "htemp", LastUpperTemp);
  }
  cJSON_AddNumberToObject(root, "ttemp", TargetTemp);
  cJSON_AddNumberToObject(root, "dryends", DryEndsAt);
  char* s = cJSON_Print(root);
  if(s){
    mqtt_publish(s);
    cJSON_free(s);
  }else{
    fprintf(stderr, "couldn't stringize JSON object\n");
  }
  cJSON_Delete(root);
}

static void
info(void){
  // version recorded by the compiler, not necessarily OTA
  const char *espv = esp_get_idf_version();
  printf("espidf version: %s\n", espv);
  const esp_app_desc_t *appdesc = esp_app_get_description();
  printf("\treported app %s version %s\n", appdesc->project_name, appdesc->version);
}

void app_main(void){
  setup(&Thermchan);
  info();
  int64_t lastpub = esp_timer_get_time();
  int64_t lasttachs = lastpub;
  while(1){
    vTaskDelay(pdMS_TO_TICKS(1000));
    float ambient = getAmbient();
    if(temp_valid_p(ambient)){
      LastLowerTemp = ambient;
    }
    float weight = getWeight();
    if(weight_valid_p(weight)){
      LastWeight = weight;
    }
    printf("esp32 temp: %f weight: %f (%svalid)\n", ambient, weight,
            weight_valid_p(weight) ? "" : "in");
    unsigned lrpm, urpm;
    printf("pwm-l: %lu pwm-u: %lu\n", LowerPWM, UpperPWM);
    printf("motor: %s heater: %s\n", motor_state(), heater_state());
    int64_t curtime = esp_timer_get_time();
    if(curtime - lasttachs > TACH_SAMPLE_QUANTUM_USEC){
      getFanTachs(&lrpm, &urpm, curtime, lasttachs);
      lasttachs = curtime;
    }
    //printf("dryends: %lld cursec: %lld\n", DryEndsAt, curtime);
    if(DryEndsAt && curtime >= DryEndsAt){
      printf("completed drying operation (%llu >= %llu)\n", curtime, DryEndsAt);
      set_motor(false);
      DryEndsAt = 0;
    }
    update_upper_temp();
    if(curtime - lastpub > MQTT_PUBLISH_QUANTUM_USEC){
      send_mqtt(curtime, lrpm, urpm);
      lastpub = curtime;
    }
  }
}
