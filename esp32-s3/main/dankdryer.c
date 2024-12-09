// intended for use on an ESP32-S3-WROOM-1
#define VERSION "0.0.9"
#define DEVICE "dankdryer"
#define CLIENTID DEVICE VERSION
#include "dryer-network.h"
#include "hx711.h"
#include "ota.h"
#include <nvs.h>
#include <math.h>
#include <mdns.h>
#include <cJSON.h>
#include <esp_wifi.h>
#include <esp_netif.h>
#include <led_strip.h>
#include <nvs_flash.h>
#include <esp_timer.h>
#include <lwip/netif.h>
#include <esp_system.h>
#include <mqtt_client.h>
#include <driver/ledc.h>
#include <esp_app_desc.h>
#include <esp_netif_sntp.h>
#include <hal/ledc_types.h>
#include <esp_idf_version.h>
#include <soc/adc_channel.h>
#include <esp_http_server.h>
#include <esp_adc/adc_cali.h>
#include <esp_adc/adc_oneshot.h>
#include <driver/temperature_sensor.h>

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

// GPIO numbers (https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/api-reference/peripherals/gpio.html)
// 0 and 3 are strapping pins
#define SSR_GPIN GPIO_NUM_1        // heater solid state relay
#define LOWER_TACHPIN GPIO_NUM_8   // lower chamber fan tachometer
#define UPPER_TACHPIN GPIO_NUM_10  // upper chamber fan tachometer
// 11-20 are connected to ADC2, which is used by wifi
// (they can still be used as GPIOs; they cannot be used for adc)
#define UPPER_PWMPIN GPIO_NUM_11   // upper chamber fan speed
#define HX711_DATAPIN GPIO_NUM_12  // hx711 data (input)
#define HX711_CLOCKPIN GPIO_NUM_13 // hx711 clock (output)
#define LOWER_PWMPIN GPIO_NUM_18   // lower chamber fan speed
// 19--20 are used for JTAG (not strictly needed)
// 26--32 are used for pstore qspi flash
#define MOTOR_RELAY GPIO_NUM_35    // enable relay for motor
#define THERM_DATAPIN GPIO_NUM_36  // analog thermometer (ADC1)
#define HALL_DATAPIN GPIO_NUM_37   // hall sensor
// 38 is sometimes the RGB neopixel
// 45 and 46 are strapping pins
#define RGB_PIN GPIO_NUM_48        // onboard RGB neopixel

#define NVS_HANDLE_NAME "pstore"
#define BOOTCOUNT_RECNAME "bootcount"
#define TAREOFFSET_RECNAME "tare"
#define LOWERPWM_RECNAME "lpwm"
#define UPPERPWM_RECNAME "upwm"

#define LOAD_CELL_MAX 5000 // 5kg capable

static const ledc_channel_t LOWER_FANCHAN = LEDC_CHANNEL_0;
static const ledc_channel_t UPPER_FANCHAN = LEDC_CHANNEL_1;
static const ledc_mode_t LEDCMODE = LEDC_LOW_SPEED_MODE; // no high-speed on S3

static bool MotorState;
static bool HeaterState;
static uint32_t LowerPWM = 128;
static uint32_t UpperPWM = 128;
static float LastWeight = -1.0;
static float TareWeight = -1.0;
static adc_channel_t Thermchan;
static bool UsePersistentStore; // set true upon successful initialization
static time_t DryEndsAt; // dry stop time in seconds since epoch
static uint32_t TargetTemp; // valid iff DryEndsAt != 0
static unsigned LastLowerRPM, LastUpperRPM;
static float LastLowerTemp, LastUpperTemp;
static HX711 hx711;

// ISR counters
static uint32_t LowerPulses, UpperPulses; // tach signals recorded

// ESP-IDF objects
static bool ADC1Calibrated;
static httpd_handle_t HTTPServ;
static led_strip_handle_t Neopixel;
static adc_oneshot_unit_handle_t ADC1;
static temperature_sensor_handle_t temp;
static adc_cali_handle_t ADC1Calibration;
static esp_mqtt_client_handle_t MQTTHandle;

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

static enum {
  WIFI_INVALID,
  WIFI_CONNECTING,
  NET_CONNECTING,
  MQTT_CONNECTING,
  MQTT_ESTABLISHED,
  NETWORK_STATE_COUNT
} NetworkState;

static bool StartupFailure;
static const failure_indication SystemError = { 32, 0, 0 };
static const failure_indication NetworkError = { 32, 0, 32 };
static const failure_indication PostFailure = { 0, 0, 0 };
/*static const failure_indication NetworkIndications[NETWORK_STATE_COUNT] = {
  { 0, 255, 0 },  // WIFI_INVALID
  { 0, 192, 0 },  // WIFI_CONNECTING
  { 0, 128, 0 },  // NET_CONNECTING
  { 0, 64, 0 },   // MQTT_CONNECTING
  { 0, 0, 0 }     // MQTT_ESTABLISHED
};*/

static const esp_mqtt_client_config_t MQTTConfig = {
  .broker = {
    .address = {
      .uri = MQTTURI,
    },
  },
  .credentials = {
    .username = MQTTUSER,
    // FIXME set client id, but stir in some per-device data
    .authentication = {
      .password = MQTTPASS,
    },
  },
};

static void
tach_isr(void* pulsecount){
  uint32_t* pc = pulsecount;
  ++*pc;
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

static int
extract_bool(const char* data, size_t dlen, bool* val){
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
static int
extract_pwm(const char* data, size_t dlen){
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

// set the desired PWM value
static int
set_pwm(const ledc_channel_t channel, unsigned pwm){
  if(ledc_set_duty(LEDCMODE, channel, pwm) != ESP_OK){
    fprintf(stderr, "error setting pwm!\n");
    return -1;
  }else if(ledc_update_duty(LEDCMODE, channel) != ESP_OK){
    fprintf(stderr, "error committing pwm!\n");
    return -1;
  }
  printf("set pwm to %u on channel %d\n", pwm, channel);
  return 0;
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
  if(hx711_read(&hx711, &v) || v < 0){
    fprintf(stderr, "bad hx711 read %ld\n", v);
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
int nvs_get_opt_float(nvs_handle_t nh, const char* recname, float* val){
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
int read_pstore(void){
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

int initialize_pwm(ledc_channel_t channel, gpio_num_t pin, int freq, ledc_timer_t timer){
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
    fprintf(stderr, "error (timer config)!\n");
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
    fprintf(stderr, "error (channel config)!\n");
    return -1;
  }
  return 0;
}

int initialize_25k_pwm(ledc_channel_t channel, gpio_num_t pin, ledc_timer_t timer){
  return initialize_pwm(channel, pin, 25000, timer);
}

int initialize_tach(gpio_num_t pin, uint32_t* arg){
  if(gpio_set_input(pin)){
    return -1;
  }
  esp_err_t e = gpio_set_intr_type(pin, GPIO_INTR_NEGEDGE);
  if(e != ESP_OK){
    fprintf(stderr, "failure (%s) installing %d interrupt\n", esp_err_to_name(e), pin);
    return -1;
  }
  if((e = gpio_isr_handler_add(pin, tach_isr, arg)) != ESP_OK){
    fprintf(stderr, "failure (%s) setting %d isr\n", esp_err_to_name(e), pin);
    return -1;
  }
  if((e = gpio_intr_enable(pin)) != ESP_OK){
    fprintf(stderr, "failure (%s) enabling %d interrupt\n", esp_err_to_name(e), pin);
    return -1;
  }
  return 0;
}

int setup_fans(gpio_num_t lowerppin, gpio_num_t upperppin,
               gpio_num_t lowertpin, gpio_num_t uppertpin){
  if(initialize_tach(lowertpin, &LowerPulses)){
    return -1;
  }
  if(initialize_tach(uppertpin, &UpperPulses)){
    return -1;
  }
  if(initialize_25k_pwm(LOWER_FANCHAN, lowerppin, LEDC_TIMER_1)){
    return -1;
  }
  if(initialize_25k_pwm(UPPER_FANCHAN, upperppin, LEDC_TIMER_2)){
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

static void
set_network_state(int state){
  // FIXME lock
  NetworkState = state;
  if(state != WIFI_INVALID){ // if invalid, leave any initial failure status up
    if(state < NETWORK_STATE_COUNT){
      //set_led(&NetworkIndications[state]);
    }
  }
}

static void
wifi_event_handler(void* arg, esp_event_base_t base, int32_t id, void* data){
  esp_err_t err;
  if(strcmp(base, WIFI_EVENT)){
    fprintf(stderr, "non-wifi event %s in wifi handler\n", base);
    return;
  }
  if(id == WIFI_EVENT_STA_START || id == WIFI_EVENT_STA_DISCONNECTED){
    set_network_state(WIFI_CONNECTING);
    if((err = esp_wifi_connect()) != ESP_OK){
      fprintf(stderr, "error (%s) connecting to wifi\n", esp_err_to_name(err));
    }else{
      printf("attempting to connect to wifi\n");
    }
  }else if(id == WIFI_EVENT_STA_CONNECTED){
    set_network_state(NET_CONNECTING);
    printf("connected to wifi, looking for ip\n");
    uint16_t aid = 65535u;
    esp_wifi_sta_get_aid(&aid);
    printf("association id: %u\n", aid);
  }else if(id == WIFI_EVENT_HOME_CHANNEL_CHANGE){
    printf("wifi channel changed\n");
  }else{
    fprintf(stderr, "unknown wifi event %ld\n", id);
  }
}

static void
ip_event_handler(void* arg, esp_event_base_t base, int32_t id, void* data){
  esp_err_t err;
  if(strcmp(base, IP_EVENT)){
    fprintf(stderr, "non-ip event %s in ip handler\n", base);
    return;
  }
  if(id == IP_EVENT_STA_GOT_IP || id == IP_EVENT_GOT_IP6){
    printf("got network address, connecting to mqtt/sntp\n");
    if((err = esp_netif_sntp_start()) != ESP_OK){
      fprintf(stderr, "error (%s) starting SNTP\n", esp_err_to_name(err));
    }
    set_network_state(MQTT_CONNECTING);
    if((err = esp_mqtt_client_start(MQTTHandle)) != ESP_OK){
      fprintf(stderr, "error (%s) connecting to mqtt\n", esp_err_to_name(err));
    }
  }else if(id == IP_EVENT_STA_LOST_IP){
    fprintf(stderr, "lost ip address\n");
  }else{
    fprintf(stderr, "unknown ip event %ld\n", id);
  }
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
bool_as_onoff_http(bool b){
  return b ? "<font color=\"green\">on</font>" : "off";
}

static inline const char*
motor_state(void){
  return bool_as_onoff(MotorState);
}

static inline const char*
motor_state_http(void){
  return bool_as_onoff_http(MotorState);
}

static inline const char*
heater_state(void){
  return bool_as_onoff(HeaterState);
}

static inline const char*
heater_state_http(void){
  return bool_as_onoff_http(HeaterState);
}

static void
set_motor(bool enabled){
  MotorState = enabled;
  gpio_level(MOTOR_RELAY, enabled);
  printf("set motor %s\n", motor_state());
}

static void
set_heater(bool enabled){
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

#define CCHAN "control/"
#define MOTOR_CHANNEL CCHAN DEVICE "/motor"
#define HEATER_CHANNEL CCHAN DEVICE "/heater"
#define LPWM_CHANNEL CCHAN DEVICE "/lpwm"
#define UPWM_CHANNEL CCHAN DEVICE "/upwm"
#define DRY_CHANNEL CCHAN DEVICE "/dry"
#define TARE_CHANNEL CCHAN DEVICE "/tare"
#define CALIBRATE_CHANNEL CCHAN DEVICE "/calibrate"
#define FACTORYRESET_CHANNEL CCHAN DEVICE "/factoryreset"

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
handle_dry(const char* payload, size_t plen){
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
  if(temp > MAX_DRYREQ_TMP || temp < MIN_DRYREQ_TMP){
    goto err;
  }
  printf("dry request for %us at %uC\n", seconds, temp);
  DryEndsAt = esp_timer_get_time() + seconds * 1000000ull;
  set_motor(seconds != 0);
  TargetTemp = temp;
  update_upper_temp();
  return 0;

err:
  fprintf(stderr, "invalid dry payload [%.*s]\n", plen, payload);
  return -1;
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

static void
factory_reset(void){
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

void handle_mqtt_msg(const esp_mqtt_event_t* e){
  printf("control message [%.*s] [%.*s]\n", e->topic_len, e->topic, e->data_len, e->data);
  if(topic_matches(e, DRY_CHANNEL)){
    handle_dry(e->data, e->data_len);
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
      set_heater(heater);
    }
  }else if(topic_matches(e, LPWM_CHANNEL)){
    int pwm = extract_pwm(e->data, e->data_len);
    if(pwm >= 0){
      LowerPWM = pwm;
      write_pwm(LOWERPWM_RECNAME, pwm);
      set_pwm(LOWER_FANCHAN, LowerPWM);
    }
  }else if(topic_matches(e, UPWM_CHANNEL)){
    int pwm = extract_pwm(e->data, e->data_len);
    if(pwm >= 0){
      UpperPWM = pwm;
      write_pwm(UPPERPWM_RECNAME, pwm);
      set_pwm(UPPER_FANCHAN, UpperPWM);
    }
  }else if(topic_matches(e, TARE_CHANNEL)){
    if(weight_valid_p(LastWeight)){
      TareWeight = LastWeight;
      write_tare_offset(TareWeight);
      printf("tared at %f\n", TareWeight);
    }else{
      fprintf(stderr, "requested tare, but no valid measurements yet\n");
    }
  }else if(topic_matches(e, CALIBRATE_CHANNEL)){
    // FIXME get value, match against LastWeight - TareWeight
  }else if(topic_matches(e, FACTORYRESET_CHANNEL)){
    factory_reset();
    // ought not reach here
  }else{
    fprintf(stderr, "unknown topic [%.*s], ignoring message\n", e->topic_len, e->topic);
  }
}

static void
subscribe(esp_mqtt_client_handle_t handle, const char* chan){
  int er;
  if((er = esp_mqtt_client_subscribe(handle, chan, 0)) < 0){
    fprintf(stderr, "failure %d subscribing to %s\n", er, chan);
  }
}

void mqtt_event_handler(void* arg, esp_event_base_t base, int32_t id, void* data){
  if(id == MQTT_EVENT_CONNECTED){
    printf("connected to mqtt\n");
    set_network_state(MQTT_ESTABLISHED);
    subscribe(MQTTHandle, MOTOR_CHANNEL);
    subscribe(MQTTHandle, LPWM_CHANNEL);
    subscribe(MQTTHandle, UPWM_CHANNEL);
    subscribe(MQTTHandle, DRY_CHANNEL);
    subscribe(MQTTHandle, TARE_CHANNEL);
    subscribe(MQTTHandle, CALIBRATE_CHANNEL);
    subscribe(MQTTHandle, FACTORYRESET_CHANNEL);
  }else if(id == MQTT_EVENT_DATA){
    handle_mqtt_msg(data);
  }else{
    printf("unhandled mqtt event %ld\n", id);
  }
}

int setup_mdns(void){
  esp_err_t err;
  if((err = mdns_init()) != ESP_OK){
    fprintf(stderr, "failure %d (%s) initializing mDNS\n", err, esp_err_to_name(err));
    return -1;
  }
  if((err = mdns_hostname_set(CLIENTID)) != ESP_OK){
    fprintf(stderr, "failure %d (%s) initializing mDNS\n", err, esp_err_to_name(err));
    return -1;
  }
  mdns_instance_name_set("Dankdryer");
  mdns_service_add(NULL, "_http", "_tcp", 80, NULL, 0);
  mdns_service_instance_name_set("_http", "_tcp", "Dankdryer webserver");
  return 0;
}

static esp_err_t
httpd_get_handler(httpd_req_t *req){
#define RESPBYTES 1024
  char* resp = malloc(RESPBYTES);
  if(!resp){
    fprintf(stderr, "couldn't allocate httpd response\n");
    return ESP_FAIL;
  }
  time_t now = time(NULL);
  // FIXME need locking or atomics now
  int slen = snprintf(resp, RESPBYTES, "<!DOCTYPE html><html><head><title>" DEVICE "</title></head>"
            "<body><h2>a drying comes across the sky</h2><br/>"
            "lpwm: %lu upwm: %lu<br/>"
            "lrpm: %u urpm: %u<br/>"
            "motor: %s heater: %s<br/>"
            "mass: %f tare: %f<br/>"
            "lm35: %f esp32s3: %f<br/>"
            "dryends: %llu<br/>"
            "target temp: %lu<br/>"
            "<hr/>%s<br/>"
            "</body></html>",
            LowerPWM, UpperPWM,
            LastLowerRPM, LastUpperRPM,
            motor_state_http(),
            heater_state_http(),
            LastWeight, TareWeight,
            LastUpperTemp, LastLowerTemp,
            DryEndsAt,
            TargetTemp,
            asctime(localtime(&now))
            );
  esp_err_t ret = ESP_FAIL;
  if(slen < 0 || slen >= RESPBYTES){
    fprintf(stderr, "httpd response too large (%d)\n", slen);
  }else{
    esp_err_t e = httpd_resp_send(req, resp, slen);
    if(e != ESP_OK){
      fprintf(stderr, "failure (%s) sending http response\n", esp_err_to_name(e));
    }else{
      ret = ESP_OK;
    }
  }
  free(resp);
  return ret;
}

static int
setup_httpd(void){
  httpd_config_t hconf = HTTPD_DEFAULT_CONFIG();
  esp_err_t err;
  if((err = httpd_start(&HTTPServ, &hconf)) != ESP_OK){
    fprintf(stderr, "failure (%s) initializing httpd\n", esp_err_to_name(err));
    return -1;
  }
  const httpd_uri_t httpd_get = {
    .uri = "/",
    .method = HTTP_GET,
    .handler = httpd_get_handler,
    .user_ctx = NULL,
  };
  if((err = httpd_register_uri_handler(HTTPServ, &httpd_get)) != ESP_OK){
    fprintf(stderr, "failure (%s) preparing URI %s\n", esp_err_to_name(err), httpd_get.uri);
    return -1;
  }
  return 0;
}

// we want to use the NTP servers provided by DHCP, so don't provide any
// static ones
static int
setup_sntp(void){
  esp_sntp_config_t sconf = ESP_NETIF_SNTP_DEFAULT_CONFIG_MULTIPLE(0, {});
  sconf.start = false;
  sconf.server_from_dhcp = true;
  sconf.renew_servers_after_new_IP = true;
  sconf.ip_event_to_renew = IP_EVENT_STA_GOT_IP;
  esp_err_t e = esp_netif_sntp_init(&sconf);
  if(e != ESP_OK){
    fprintf(stderr, "error (%s) initializing SNTP\n", esp_err_to_name(e));
    return -1;
  }
  return 0;
}

static int
setup_network(void){
  if((MQTTHandle = esp_mqtt_client_init(&MQTTConfig)) == NULL){
    fprintf(stderr, "couldn't create mqtt client\n");
    return -1;
  }
  esp_err_t err;
  const wifi_init_config_t wificfg = WIFI_INIT_CONFIG_DEFAULT();
  wifi_config_t stacfg = {
    .sta = {
        .ssid = WIFIESSID,
        .password = WIFIPASS,
        .sort_method = WIFI_CONNECT_AP_BY_SECURITY,
        .threshold = {
            .authmode = WIFI_AUTH_WPA2_PSK,
        },
        .sae_pwe_h2e = WPA3_SAE_PWE_HUNT_AND_PECK,
        .sae_h2e_identifier = "",
    },
  };
  if((err = esp_netif_init()) != ESP_OK){
    fprintf(stderr, "failure (%s) initializing tcp/ip\n", esp_err_to_name(err));
    return -1;
  }
  if((err = esp_event_loop_create_default()) != ESP_OK){
    fprintf(stderr, "failure (%s) creating loop\n", esp_err_to_name(err));
    return -1;
  }
  if(!esp_netif_create_default_wifi_sta()){
    fprintf(stderr, "failure creating default STA\n");
    return -1;
  }
  if((err = esp_wifi_init(&wificfg)) != ESP_OK){
    fprintf(stderr, "failure (%s) initializing wifi\n", esp_err_to_name(err));
    return -1;
  }
  esp_event_handler_instance_t wid;
  if((err = esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_event_handler, NULL, &wid)) != ESP_OK){
    fprintf(stderr, "failure (%s) registering wifi events\n", esp_err_to_name(err));
    return -1;
  }
  esp_event_handler_instance_t ipd;
  if((err = esp_event_handler_instance_register(IP_EVENT, ESP_EVENT_ANY_ID, ip_event_handler, NULL, &ipd)) != ESP_OK){
    fprintf(stderr, "failure (%s) registering ip events\n", esp_err_to_name(err));
    return -1;
  }
  if((err = esp_mqtt_client_register_event(MQTTHandle, MQTT_EVENT_CONNECTED, mqtt_event_handler, NULL)) != ESP_OK){
    fprintf(stderr, "failure (%s) registering mqtt events\n", esp_err_to_name(err));
    return -1;
  }
  if((err = esp_mqtt_client_register_event(MQTTHandle, MQTT_EVENT_DATA, mqtt_event_handler, NULL)) != ESP_OK){
    fprintf(stderr, "failure (%s) registering mqtt events\n", esp_err_to_name(err));
    return -1;
  }
  if((err = esp_wifi_set_mode(WIFI_MODE_STA)) != ESP_OK){
    fprintf(stderr, "failure (%s) setting STA mode\n", esp_err_to_name(err));
    return -1;
  }
  if((err = esp_wifi_set_config(WIFI_IF_STA, &stacfg)) != ESP_OK){
    fprintf(stderr, "failure (%s) configuring wifi\n", esp_err_to_name(err));
    return -1;
  }
  if((err = esp_wifi_start()) != ESP_OK){
    fprintf(stderr, "failure (%s) starting wifi\n", esp_err_to_name(err));
    return -1;
  }
  const int8_t txpower = 84;
  if((err = esp_wifi_set_max_tx_power(txpower)) != ESP_OK){
    fprintf(stderr, "warning: error (%s) setting tx power to %hd\n", esp_err_to_name(err), txpower);
  }
  setup_sntp(); // allow a failure
  setup_mdns(); // allow a failure
  if(setup_httpd()){
    return -1;
  }
  return 0;
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

static void
setup(adc_channel_t* thermchan){
  setup_neopixel(RGB_PIN);
  printf("dankdryer v" VERSION "\n");
  if(!init_pstore()){
    if(!read_pstore()){
      UsePersistentStore = true;
    }
  }
  if(ota_init()){
    set_failure(&SystemError);
  }
  esp_err_t e = gpio_install_isr_service(0);
  if(e != ESP_OK){
    fprintf(stderr, "error (%s) installing isr service\n", esp_err_to_name(e));
    set_failure(&SystemError);
  }
  if(setup_adc_oneshot(ADC_UNIT_1, &ADC1, &ADC1Calibration, &ADC1Calibrated)){
    set_failure(&SystemError);
  }
  if(setup_temp(THERM_DATAPIN, ADC_UNIT_1, thermchan)){
    set_failure(&SystemError);
  }
  if(hx711_init(&hx711, HX711_DATAPIN, HX711_CLOCKPIN)){
    set_failure(&SystemError);
  }
  if(setup_fans(LOWER_PWMPIN, UPPER_PWMPIN, LOWER_TACHPIN, UPPER_TACHPIN)){
    set_failure(&SystemError);
  }
  if(setup_heater(SSR_GPIN)){
    set_failure(&SystemError);
  }
  if(setup_motor(MOTOR_RELAY)){
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
  *lrpm = LowerPulses;
  *urpm = UpperPulses;
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
  LowerPulses = 0;
  UpperPulses = 0;
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
    size_t slen = strlen(s);
    printf("MQTT: %s\n", s);
    if(esp_mqtt_client_publish(MQTTHandle, MQTTTOPIC, s, slen, 0, 0)){
      fprintf(stderr, "couldn't publish %zuB mqtt message\n", slen);
    }
    cJSON_free(s);
  }else{
    fprintf(stderr, "couldn't stringize JSON object\n");
  }
  cJSON_Delete(root);
}

static void
info(void){
  const char *espv = esp_get_idf_version();
  printf("espidf version: %s\n", espv);
  const esp_app_desc_t *appdesc = esp_app_get_description();
  printf("esp reported app %s version %s\n", appdesc->project_name, appdesc->version);
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
