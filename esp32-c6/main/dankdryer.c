// intended for use on an ESP32-S3-WROOM-1
#include "networking.h"
#include "dankdryer.h"
#include "version.h"
#include "nau7802.h"
#include "heater.h"
#include "efuse.h"
#include "reset.h"
#include "pins.h"
#include "fans.h"
#include "ota.h"
#include <nvs.h>
#include <time.h>
#include <math.h>
#include <ctype.h>
#include <cJSON.h>
#include <string.h>
#include <esp_log.h>
#include <stdatomic.h>
#include <led_strip.h>
#include <nvs_flash.h>
#include <esp_timer.h>
#include <esp_system.h>
#include <driver/ledc.h>
#include <driver/gpio.h>
#include <esp_app_desc.h>
#include <hal/ledc_types.h>
#include <esp_idf_version.h>
//#include <esp32c6/rom/rtc.h>
#include <esp_adc/adc_cali.h>
#include <driver/i2c_master.h>
#include <driver/gpio_filter.h>
#include <driver/temperature_sensor.h>

#define TAG "main"

#define UUIDLEN 16
#define MAX_DRYREQ_TMP 150
#define MIN_DRYREQ_TMP 50
// minimum of 15s between mqtt publications
#define MQTT_PUBLISH_QUANTUM_USEC 15000000ul
// give tachs a 5s period to smooth them out
#define TACH_SAMPLE_QUANTUM_USEC 5000000ul

#define BOOTCOUNT_RECNAME "bootcount"
#define TAREOFFSET_RECNAME "tare"
#define ESSID_RECNAME "essid"
#define PSK_RECNAME "psk"
#define SETUPSTATE_RECNAME "sstate"
#define LOAD_CELL_MAX 5000 // 5kg capable

static bool MotorState;
static bool StartupFailure;
static float LastWeight = -1.0;
static float TareWeight = -1.0;
static time_t DryEndsAt; // dry stop time in seconds since epoch
static uint32_t Bootcount;  // preserved across factory reset
static float LastLowerTemp;
static uint32_t TargetTemp; // meaningful iff DryEndsAt != 0
static uint32_t LastSpoolRPM;
static i2c_master_dev_handle_t NAU7802;
static i2c_master_bus_handle_t I2CMaster;
static uint32_t LastLowerRPM, LastUpperRPM;

// variables manipulated by interrupts
static _Atomic(uint32_t) HallPulses;

// ESP-IDF objects
static bool NAUAvailable;
static temperature_sensor_handle_t temp;

static void
pulse_isr(void* pulsecount){
  _Atomic(uint32_t)* pc = pulsecount;
  ++*pc;
}

static inline bool
rpm_valid_p(unsigned rpm){
  return rpm < 3000;
}

static inline bool
weight_valid_p(float weight){
  return weight >= 0 && weight <= LOAD_CELL_MAX;
}

int setup_intr(gpio_num_t pin, _Atomic(uint32_t)* arg){
  if(gpio_set_input(pin)){
    return -1;
  }
  esp_err_t e;
  gpio_pin_glitch_filter_config_t fconf = {
    .clk_src = SOC_MOD_CLK_XTAL,
    .gpio_num = pin,
  };
  gpio_glitch_filter_handle_t gfilter;
  if((e = gpio_new_pin_glitch_filter(&fconf, &gfilter)) != ESP_OK){
    ESP_LOGE(TAG, "error (%s) creating glitch filter on %d", esp_err_to_name(e), pin);
    return -1;
  }
  if((e = gpio_glitch_filter_enable(gfilter)) != ESP_OK){
    ESP_LOGE(TAG, "error (%s) enabling glitch filter on %d", esp_err_to_name(e), pin);
    return -1;
  }
  if((e = gpio_pullup_dis(pin)) != ESP_OK){
    ESP_LOGE(TAG, "error (%s) disabling pullup on %d", esp_err_to_name(e), pin);
    return -1;
  }
  if((e = gpio_pulldown_en(pin)) != ESP_OK){
    ESP_LOGE(TAG, "error (%s) enabling pulldown on %d", esp_err_to_name(e), pin);
    return -1;
  }
  if((e = gpio_set_intr_type(pin, GPIO_INTR_NEGEDGE)) != ESP_OK){
    ESP_LOGE(TAG, "failure (%s) installing %d interrupt", esp_err_to_name(e), pin);
    return -1;
  }
  if((e = gpio_isr_handler_add(pin, pulse_isr, arg)) != ESP_OK){
    ESP_LOGE(TAG, "failure (%s) setting %d isr", esp_err_to_name(e), pin);
    return -1;
  }
  if((e = gpio_intr_enable(pin)) != ESP_OK){
    ESP_LOGE(TAG, "failure (%s) enabling %d interrupt", esp_err_to_name(e), pin);
    return -1;
  }
  return 0;
}

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

uint32_t get_spool_rpm(void){
  return LastSpoolRPM;
}

unsigned get_lower_rpm(void){
  return LastLowerRPM;
}

unsigned get_upper_rpm(void){
  return LastUpperRPM;
}

int read_wifi_config(unsigned char* essid, size_t essidlen,
                     unsigned char* psk, size_t psklen,
                     int* setupstate){
  nvs_handle_t nvsh;
  esp_err_t err = nvs_open(NVS_HANDLE_NAME, NVS_READWRITE, &nvsh);
  if(err){
    ESP_LOGE(TAG, "failure (%d) opening nvs:" NVS_HANDLE_NAME "", err);
    return -1;
  }
  if(nvs_get_str(nvsh, ESSID_RECNAME, (char*)essid, &essidlen) != ESP_OK){
    ESP_LOGE(TAG, "failure (%s) reading essid", esp_err_to_name(err));
    goto err;
  }
  if(nvs_get_str(nvsh, PSK_RECNAME, (char*)psk, &psklen) != ESP_OK){
    ESP_LOGE(TAG, "failure (%s) reading psk", esp_err_to_name(err));
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
      ESP_LOGE(TAG, "read invalid setup state %lu", rawstate);
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
    ESP_LOGE(TAG, "error (%s) opening nvs:" NVS_HANDLE_NAME, esp_err_to_name(err));
    return -1;
  }
  err = nvs_set_str(nvsh, ESSID_RECNAME, (const char*)essid);
  if(err){
    ESP_LOGE(TAG, "error (%s) writing " NVS_HANDLE_NAME ":%s", esp_err_to_name(err), ESSID_RECNAME);
    nvs_close(nvsh);
    return -1;
  }
  err = nvs_set_str(nvsh, PSK_RECNAME, (const char*)psk);
  if(err){
    ESP_LOGE(TAG, "error (%s) writing " NVS_HANDLE_NAME ":%s", esp_err_to_name(err), PSK_RECNAME);
    nvs_close(nvsh);
    return -1;
  }
  err = nvs_set_u32(nvsh, SETUPSTATE_RECNAME, state);
  if(err){
    ESP_LOGE(TAG, "error (%s) writing " NVS_HANDLE_NAME ":%s", esp_err_to_name(err), SETUPSTATE_RECNAME);
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

// update NVS with tare offset
static int
write_tare_offset(float tare){
  nvs_handle_t nvsh;
  esp_err_t err = nvs_open(NVS_HANDLE_NAME, NVS_READWRITE, &nvsh);
  if(err){
    ESP_LOGE(TAG, "error (%s) opening nvs:" NVS_HANDLE_NAME, esp_err_to_name(err));
    return -1;
  }
  char buf[20];
  if(snprintf(buf, sizeof(buf), "%.8f", tare) > sizeof(buf)){
    ESP_LOGW(TAG, "warning: couldn't store tare offset %f in buf", tare);
  }
  err = nvs_set_str(nvsh, TAREOFFSET_RECNAME, buf);
  if(err){
    ESP_LOGE(TAG, "error (%s) writing " NVS_HANDLE_NAME ":" TAREOFFSET_RECNAME, esp_err_to_name(err));
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

void set_tare(void){
  if(weight_valid_p(LastWeight)){
    TareWeight = LastWeight;
    write_tare_offset(TareWeight);
    ESP_LOGI(TAG, "tared at %f", TareWeight);
  }else{
    ESP_LOGE(TAG, "requested tare, but no valid measurements yet");
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
    ESP_LOGE(TAG, "failed acquiring temperature");
    return MIN_TEMP - 1;
  }
  return t;
}

static int
setup_nau7802(i2c_master_bus_handle_t master){
  if(nau7802_detect(master, &NAU7802)){
    return -1;
  }
  if(nau7802_poweron(NAU7802)){
    return -1;
  }
  if(nau7802_set_pga_cap(NAU7802, true)){
    return -1;
  }
  if(nau7802_enable_ldo(NAU7802, NAU7802_LDO_33V, true)){
    return -1;
  }
  // FIXME set gain?
  NAUAvailable = true;
  return 0;
}

float getWeight(void){
  float v;
  if(!NAUAvailable){
    if(setup_nau7802(I2CMaster)){
      return -1.0;
    }
  }
  if(nau7802_read_scaled(NAU7802, &v, LOAD_CELL_MAX)){
    return -1.0;
  }
  v += LOAD_CELL_MAX / 2;
  if(v > LOAD_CELL_MAX || v < 0){
    ESP_LOGE(TAG, "bad nau7802 read %f", v);
    return -1.0;
  }
  float tare = 0;
  if(weight_valid_p(TareWeight)){
    tare = TareWeight;
  }
  float tared = v - tare;
  ESP_LOGI(TAG, "tare (%f) %f to %f", tare, v, tared);
  return tared;
}

static int
init_pstore(void){
  ESP_LOGI(TAG, "initializing persistent store...");
  esp_err_t err = nvs_flash_init();
  if(err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND){
    printf("erasing flash...\n");
    err = nvs_flash_erase();
    if(err == ESP_OK){
      ESP_LOGI(TAG, "initializing flash...");
      err = nvs_flash_init();
    }
  }
  if(err){
    ESP_LOGE(TAG, "error (%s) initializing nvs", esp_err_to_name(err));
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
    ESP_LOGE(TAG, "failure (%d) reading %s", err, recname);
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
    ESP_LOGI(TAG, "no record '%s' in nvs", recname);
    return -1;
  }else if(err){
    ESP_LOGE(TAG, "error (%s) reading %s", esp_err_to_name(err), recname);
    return -1;
  }
  char* bend;
  *val = strtof(buf, &bend);
  if(bend || !*val || isnan(*val)){
    ESP_LOGE(TAG, "couldn't convert [%s] to float for nvs:%s", buf, recname);
    return -1;
  }
  ESP_LOGI(TAG, "read configured default %f from nvs:%s", *val, recname);
  return 0;
}

// read and update boot count, read configurable defaults from pstore if they
// are present (we do not write defaults to pstore, so we can differentiate
// between defaults and a configured value). bootcount can be specified so
// that it can be preserved across a factory reset.
static int
read_pstore(void){
  nvs_handle_t nvsh;
  esp_err_t err = nvs_open(NVS_HANDLE_NAME, NVS_READWRITE, &nvsh);
  if(err){
    ESP_LOGE(TAG, "error (%s) opening nvs:" NVS_HANDLE_NAME, esp_err_to_name(err));
    return -1;
  }
  err = nvs_get_u32(nvsh, BOOTCOUNT_RECNAME, &Bootcount);
  if(err && err != ESP_ERR_NVS_NOT_FOUND){
    ESP_LOGE(TAG, "error (%s) reading " NVS_HANDLE_NAME ":" BOOTCOUNT_RECNAME, esp_err_to_name(err));
    nvs_close(nvsh);
    return -1;
  }
  ++Bootcount;
  ESP_LOGI(TAG, "this is boot #%" PRIu32, Bootcount);
  err = nvs_set_u32(nvsh, BOOTCOUNT_RECNAME, Bootcount);
  if(err){
    ESP_LOGE(TAG, "error (%s) writing " NVS_HANDLE_NAME ":" BOOTCOUNT_RECNAME, esp_err_to_name(err));
    nvs_close(nvsh);
    return -1;
  }
  err = nvs_commit(nvsh);
  if(err){
    ESP_LOGE(TAG, "error (%s) committing nvs:" NVS_HANDLE_NAME, esp_err_to_name(err));
    nvs_close(nvsh);
    return -1;
  }
  read_fans_pstore(nvsh);
  float tare = TareWeight; // if not present, don't change initialized value
  if(nvs_get_opt_float(nvsh, TAREOFFSET_RECNAME, &tare) == 0){
    if(weight_valid_p(tare)){
      TareWeight = tare;
    }else{
      ESP_LOGE(TAG, "read invalid tare offset %f", tare);
    }
  }
  nvs_close(nvsh);
  return 0;
}

static void
set_failure(void){
  StartupFailure = true;
}

static inline const char*
motor_state(void){
  return bool_as_onoff(MotorState);
}

static inline const char*
heater_state_str(void){
  return bool_as_onoff(get_heater_state());
}

bool get_motor_state(void){
  return MotorState;
}

void set_motor(bool enabled){
  MotorState = enabled;
  gpio_level(MOTOR_GATEPIN, enabled);
  printf("set motor %s\n", motor_state());
}

int handle_dry(unsigned seconds, unsigned temp){
  printf("dry request for %us at %uC\n", seconds, temp);
  if(temp > MAX_DRYREQ_TMP || temp < MIN_DRYREQ_TMP){
    ESP_LOGE(TAG, "invalid temp request (%u)", temp);
    return -1;
  }
  DryEndsAt = esp_timer_get_time() + seconds * 1000000ull;
  set_motor(seconds != 0);
  TargetTemp = temp;
  manage_heater(SSR_GPIN, DryEndsAt, TargetTemp);
  return 0;
}

// FIXME ought preserve boot count
void factory_reset(void){
  printf("requested factory reset, erasing nvs...\n");
  set_motor(false);
  set_heater(SSR_GPIN, false);
  esp_err_t e = nvs_flash_erase();
  if(e != ESP_OK){
    ESP_LOGE(TAG, "error (%s) erasing nvs", esp_err_to_name(e));
  }
  if(!init_pstore()){
    read_pstore();
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
  set_heater(hrelaypin, false);
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
    ESP_LOGE(TAG, "error (%s) initializing i2c master", esp_err_to_name(e));
    return -1;
  }
  return 0;
}

static void
print_reset_reason(void){
  esp_reset_reason_t r = esp_reset_reason();
  const char* s = r == ESP_RST_POWERON ?
    "power on" : r == ESP_RST_SW ?
    "esp_restart()" : r == ESP_RST_PANIC ?
    "panic" : "unknown"; // FIXME there are more
  ESP_LOGI(TAG, "reset code %d (%s)", r, s);
}

static void
setup(void){
  ESP_LOGI(TAG, DEVICE " v" VERSION);
  print_reset_reason();
  if(!init_pstore()){
    read_pstore();
  }
  if(ota_init()){
    set_failure();
  }
  // install the ETS_GPIO_INTR_SOURCE interrupt handler, which demuxes
  // to per-pin interrupt handlers.
  esp_err_t e = gpio_install_isr_service(0);
  if(e != ESP_OK){
    ESP_LOGE(TAG, "error (%s) installing isr service", esp_err_to_name(e));
    set_failure();
  }
  if(setup_intr(HALL_DATAPIN, &HallPulses)){
    set_failure();
  }
  if(setup_factory_reset(FRESET_PIN)){
    set_failure();
  }
  if(setup_fans(LOWER_PWMPIN, UPPER_PWMPIN, LOWER_TACHPIN, UPPER_TACHPIN)){
    set_failure();
  }
  if(setup_i2c(&I2CMaster, SDA_PIN, SCL_PIN)){
    set_failure();
  }
  /*if(setup_lcd(LCD_SDA_PIN, LCD_SCL_PIN, LCD_DC_PIN, LCD_CS_PIN, LCD_RST_PIN)){
    set_failure();
  }*/
  temperature_sensor_config_t conf = TEMPERATURE_SENSOR_CONFIG_DEFAULT(-10, 80);
  if(temperature_sensor_install(&conf, &temp) != ESP_OK){
    ESP_LOGE(TAG, "failed to set up thermostat");
    set_failure();
  }else if(temperature_sensor_enable(temp) != ESP_OK){
    ESP_LOGE(TAG, "failed to enable thermostat");
    set_failure();
  }
  if(setup_temp(THERM_DATAPIN, ADC_UNIT_1)){
    set_failure();
  }
  if(setup_heater(SSR_GPIN)){
    set_failure();
  }
  if(setup_motor(MOTOR_GATEPIN)){
    set_failure();
  }
  if(setup_network()){
    set_failure();
  }
  //gpio_dump_io_configuration(stdout, SOC_GPIO_VALID_GPIO_MASK);
  ESP_LOGI(TAG, "initialization %ssuccessful v" VERSION, StartupFailure ? "un" : "");
}

static uint32_t
get_hall_count(void){
  uint32_t r = HallPulses;
  HallPulses = 0;
  return r;
}

// we don't try to measure the first iteration, as we don't yet have a
// timestamp (and the fans are spinning up, anyway).
static void
getPulseCount(float scale, uint32_t(*getcount)(void), uint32_t *lastpcount){
  unsigned rpm;
  rpm = getcount();
  float scaled = rpm * scale;
  scaled /= 2; // two pulses for each rotation
  printf("raw: %u scale: %f rpm: %f\n", rpm, scale, scaled);
  if(rpm_valid_p(scaled)){
    *lastpcount = scaled;
  }
}

static inline bool
topic_matches(const esp_mqtt_event_t* e, const char* chan){
  if(strncmp(e->topic, chan, e->topic_len) == 0 && e->topic_len == strlen(chan)){
    return true;
  }
  return false;
}

// arguments to dry are a target temp and number of seconds in the form
// TEMP/SECONDS. a well-formed request replaces any existing one, including
// cancelling it if SECONDS is 0. we allow leading and trailing space.
static int
handle_dry_req(const char* payload, size_t plen){
  unsigned seconds = 0;
  unsigned temp = 0;
  size_t idx = 0;
  enum {
    PRESPACE,
    TEMP,
    SLASH,
    SECONDS,
    POSTSPACE
  } state = PRESPACE;
  // FIXME need address wrapping of temp and/or seconds
  while(idx < plen){
    printf("payload[%zu] = 0x%02x state: %d temp: %u\n", idx, payload[idx], state, temp);
    if(payload[idx] >= 0x80 || payload[idx] <= 0){ // invalid character
      goto err;
    }
    unsigned char c = payload[idx];
    switch(state){
      case PRESPACE:
        if(!isspace(c)){
          if(isdigit(c)){
            state = TEMP;
            temp = c - '0';
          }else{
            goto err;
          }
        }
        break;
      case TEMP:
        if(isdigit(c)){
          temp *= 10;
          temp += c - '0';
        }else if(c == '/'){
          state = SLASH;
        }else{
          goto err;
        }
        break;
      case SLASH:
        if(isdigit(c)){
          seconds = c - '0';
          state = SECONDS;
        }else{
          goto err;
        }
        break;
      case SECONDS:
        if(isdigit(c)){
          seconds *= 10;
          seconds += c - '0';
        }else if(isspace(c)){
          state = POSTSPACE;
        }else{
          goto err;
        }
        break;
      case POSTSPACE:
        if(!isspace(c)){
          goto err;
        }
        break;
    }
    ++idx;
  }
  return handle_dry(seconds, temp);

err:
  ESP_LOGE(TAG, "invalid dry payload [%.*s]", plen, payload);
  return -1;
}

void handle_mqtt_msg(const esp_mqtt_event_t* e){
  printf("control message [%.*s] [%.*s]\n", e->topic_len, e->topic, e->data_len, e->data);
  if(topic_matches(e, DRY_CHANNEL)){
    handle_dry_req(e->data, e->data_len);
  }else if(topic_matches(e, MOTOR_CHANNEL)){
    bool motor;
    int ret = extract_bool(e->data, e->data_len, &motor);
    if(ret == 0){
      set_motor(motor);
    }
  }else if(topic_matches(e, HEATER_CHANNEL)){
    bool heater;
    int ret = extract_bool(e->data, e->data_len, &heater);
    if(ret == 0){
      set_heater(SSR_GPIN, heater);
    }
  }else if(topic_matches(e, LPWM_CHANNEL)){
    int pwm = extract_pwm(e->data, e->data_len);
    if(pwm >= 0){
      set_lower_pwm(pwm);
    }
  }else if(topic_matches(e, UPWM_CHANNEL)){
    int pwm = extract_pwm(e->data, e->data_len);
    if(pwm >= 0){
      set_upper_pwm(pwm);
    }
  }else if(topic_matches(e, TARE_CHANNEL)){
    set_tare();
  }else if(topic_matches(e, OTA_CHANNEL)){
    attempt_ota();
  }else if(topic_matches(e, CALIBRATE_CHANNEL)){
    // FIXME get value, match against LastWeight - TareWeight
  }else if(topic_matches(e, FACTORYRESET_CHANNEL)){
    factory_reset();
    // ought not reach here
  }else{
    ESP_LOGE(TAG, "unknown topic [%.*s]", e->topic_len, e->topic);
  }
}

void send_mqtt(int64_t curtime){
  cJSON* root = cJSON_CreateObject();
  if(root == NULL){
    ESP_LOGE(TAG, "couldn't create JSON object\n");
    return;
  }
  cJSON_AddNumberToObject(root, "uptimesec", curtime / 1000000ll);
  if(temp_valid_p(LastLowerTemp)){
    cJSON_AddNumberToObject(root, "ltempC", LastLowerTemp);
  }
  // UINT_MAX is sentinel for known bad reading, but anything over 3KRPM on
  // these Noctua NF-A8 fans is indicative of error; they max out at 2500.
  if(rpm_valid_p(LastLowerRPM)){
    cJSON_AddNumberToObject(root, "lrpm", LastLowerRPM);
  }
  if(rpm_valid_p(LastUpperRPM)){
    cJSON_AddNumberToObject(root, "urpm", LastUpperRPM);
  }
  if(rpm_valid_p(LastSpoolRPM)){
    cJSON_AddNumberToObject(root, "srpm", LastSpoolRPM);
  }
  cJSON_AddNumberToObject(root, "lpwm", get_lower_pwm());
  cJSON_AddNumberToObject(root, "upwm", get_upper_pwm());
  if(weight_valid_p(LastWeight)){
    cJSON_AddNumberToObject(root, "mass", LastWeight);
  }
  cJSON_AddNumberToObject(root, "tare", TareWeight);
  cJSON_AddNumberToObject(root, "motor", MotorState);
  cJSON_AddNumberToObject(root, "heater", get_heater_state());
  float utemp = get_upper_temp();
  if(temp_valid_p(utemp)){
    cJSON_AddNumberToObject(root, "utempC", utemp);
  }
  cJSON_AddNumberToObject(root, "ttempC", TargetTemp);
  cJSON_AddNumberToObject(root, "dryendsec", DryEndsAt);
  char* s = cJSON_Print(root);
  if(s){
    mqtt_publish(s);
    cJSON_free(s);
  }else{
    ESP_LOGE(TAG, "couldn't stringize JSON object");
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
  // FIXME turn off heater/motor as first thing
  info();
  load_device_id();
  setup();
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
    printf("pwm-l: %u pwm-u: %u\n", get_lower_pwm(), get_upper_pwm());
    printf("motor: %s heater: %s\n", motor_state(), heater_state_str());
    int64_t curtime = esp_timer_get_time();
    if(curtime - lasttachs > TACH_SAMPLE_QUANTUM_USEC){
      const float diffu = curtime - lasttachs;
      const float scale = 60.0 * 1000000u / diffu;
			getPulseCount(scale, get_hall_count, &LastSpoolRPM);
			getPulseCount(scale, get_lower_tach, &LastLowerRPM);
			getPulseCount(scale, get_upper_tach, &LastUpperRPM);
      lasttachs = curtime;
    }
    //printf("dryends: %lld cursec: %lld\n", DryEndsAt, curtime);
    if(DryEndsAt && curtime >= DryEndsAt){
      printf("completed drying operation (%llu >= %llu)\n", curtime, DryEndsAt);
      set_motor(false);
      DryEndsAt = 0;
    }
    manage_heater(SSR_GPIN, DryEndsAt, TargetTemp);
    if(curtime - lastpub > MQTT_PUBLISH_QUANTUM_USEC){
      send_mqtt(curtime);
      lastpub = curtime;
    }
    if(check_factory_reset(curtime)){
      factory_reset();
    }
  }
}
