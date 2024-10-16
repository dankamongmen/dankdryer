// dankdryer firmware
// intended for use on an ESP32-S3-WROOM-1
#define VERSION "0.0.1"
#define DEVICE "dankdryer"
#define CLIENTID DEVICE VERSION
#include "dryer-network.h"
#include <lwip/netif.h>
#include <nvs.h>
#include <math.h>
#include <mdns.h>
#include <cJSON.h>
#include <esp_wifi.h>
#include <esp_netif.h>
#include <nvs_flash.h>
#include <esp_timer.h>
#include <esp_system.h>
#include <mqtt_client.h>
#include <driver/ledc.h>
#include <hal/ledc_types.h>
#include <esp_http_server.h>
#include <driver/temperature_sensor.h>

#define FANPWM_BIT_NUM LEDC_TIMER_8_BIT
#define RPMMAX (1u << 14u)
#define MIN_TEMP (-80)

// GPIO numbers (https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/api-reference/peripherals/gpio.html)
// 0 and 3 are strapping pins
#define MOTOR_PWMPIN GPIO_NUM_6 // motor speed
#define MOTOR_1PIN GPIO_NUM_8
#define UPPER_PWMPIN GPIO_NUM_9  // upper chamber fan speed (output)
#define MOTOR_2PIN GPIO_NUM_10
#define THERM_DATAPIN GPIO_NUM_11 // analog thermometer (input, ADC1)
#define LOWER_PWMPIN GPIO_NUM_12  // lower chamber fan speed (output)
#define MOTOR_SBYPIN GPIO_NUM_13  // standby must be taken high to drive motor (output)
// 11-20 are connected to ADC2, which is used by wifi
// (they can still be used as digital pins)
#define HX711_SCK GPIO_NUM_17    // DAC clock (i2c)
#define HX711_DT GPIO_NUM_18     // DAC data (i2c)
// 19--20 are used for JTAG (not strictly needed)
// 26--32 are used for pstore qspi flash
// 38 is used for RGB LED
#define UPPER_TACHPIN GPIO_NUM_39 // upper chamber fan tachometer (input)
// 45 and 46 are strapping pins
#define LOWER_TACHPIN GPIO_NUM_48 // lower chamber fan tachometer (input)

#define NVS_HANDLE_NAME "pstore"

static bool UsePersistentStore; // set true upon successful initialization
static temperature_sensor_handle_t temp;
static const ledc_channel_t LOWER_FANCHAN = LEDC_CHANNEL_0;
static const ledc_channel_t UPPER_FANCHAN = LEDC_CHANNEL_1;
static const ledc_channel_t MOTOR_CHAN = LEDC_CHANNEL_2;
static const ledc_mode_t LEDCMODE = LEDC_LOW_SPEED_MODE; // no high-speed on S3

// defaults, some of which can be configured.
static uint32_t MotorPWM;
static uint32_t LowerPWM = 64;
static uint32_t UpperPWM = 64;
static uint32_t TargetTemp = 80;
static float LoadcellScale = 1.0;
static httpd_handle_t HTTPServ;
static uint32_t LowerPulses, UpperPulses; // tach signals recorded
static esp_mqtt_client_handle_t MQTTHandle;

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
static const failure_indication SystemError = { 64, 0, 0 };
static const failure_indication NetworkError = { 64, 0, 64 };
static const failure_indication PostFailure = { 0, 0, 0 };
static const failure_indication NetworkIndications[NETWORK_STATE_COUNT] = {
  { 0, 255, 0 },  // WIFI_INVALID
  { 0, 192, 0 },  // WIFI_CONNECTING
  { 0, 128, 0 },  // NET_CONNECTING
  { 0, 64, 0 },   // MQTT_CONNECTING
  { 0, 0, 0 }     // MQTT_ESTABLISHED
};

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

static void IRAM_ATTR
tach_isr(void* pulsecount){
  uint32_t* pc = pulsecount;
  ++*pc;
}

static inline bool valid_pwm_p(int pwm){
  return pwm >= 0 && pwm <= 255;
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

// set the desired PWM value
int set_pwm(const ledc_channel_t channel, unsigned pwm){
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
float getAmbient(void){
  float t;
  if(temperature_sensor_get_celsius(temp, &t)){
    fprintf(stderr, "failed acquiring temperature\n");
    return MIN_TEMP - 1;
  }
  return t;
}

float getWeight(void){
  /*Load.wait_ready();
  return Load.read_average(5);*/
  return 0; // FIXME
}

// the esp32-s3 has a built in temperature sensor
int setup_esp32temp(void){
  temperature_sensor_config_t conf = TEMPERATURE_SENSOR_CONFIG_DEFAULT(-10, 80);
  if(temperature_sensor_install(&conf, &temp) != ESP_OK){
    fprintf(stderr, "failed to set up thermostat\n");
    return -1;
  }
  if(temperature_sensor_enable(temp) != ESP_OK){
    fprintf(stderr, "failed to enable thermostat\n");
    return -1;
  }
  return 0;
}

int init_pstore(void){
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
    return 0;
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
  uint32_t bootcount;
#define BOOTCOUNT_RECNAME "bootcount"
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
#undef BOOTCOUNT_RECNAME
  err = nvs_commit(nvsh);
  if(err){
    fprintf(stderr, "failure (%d) committing nvs:" NVS_HANDLE_NAME "\n", err);
    nvs_close(nvsh);
    return -1;
  }
  nvs_get_opt_u32(nvsh, "targtemp", &TargetTemp);
  nvs_get_opt_u32(nvsh, "upperfanpwm", &UpperPWM);
  nvs_get_opt_u32(nvsh, "lowerfanpwm", &LowerPWM);
  nvs_get_opt_float(nvsh, "loadcellscale", &LoadcellScale);
  // FIXME check any defaults we read and ensure they're sane
  // FIXME need we check for error here?
  nvs_close(nvsh);
  return 0;
}

int gpio_set_input(gpio_num_t pin){
  gpio_reset_pin(pin);
  esp_err_t err;
  if((err = gpio_set_direction(pin, GPIO_MODE_INPUT)) != ESP_OK){
    fprintf(stderr, "failure (%s) setting %d to input\n", esp_err_to_name(err), pin);
    return -1;
  }
  // FIXME explicitly set pullup/pulldown?
  return 0;
}

int gpio_set_output(gpio_num_t pin){
  gpio_reset_pin(pin);
  esp_err_t err;
  if((err = gpio_set_direction(pin, GPIO_MODE_OUTPUT)) != ESP_OK){
    fprintf(stderr, "failure (%s) setting %d to output\n", esp_err_to_name(err), pin);
    return -1;
  }
  // FIXME explicitly set pullup/pulldown?
  return 0;
}

int initialize_pwm(ledc_channel_t channel, gpio_num_t pin, int freq, ledc_timer_t timer){
  if(gpio_set_output(pin)){
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
  set_pwm(LOWER_FANCHAN, LowerPWM);
  set_pwm(UPPER_FANCHAN, UpperPWM);
  return 0;
}

static void
set_led(const struct failure_indication *nin){
  fprintf(stderr, "FIXME we don't yet have neopixel support\n");
  // FIXME neopixelWrite(RGB_BUILTIN, nin->r, nin->g, nin->b);
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
      set_led(&NetworkIndications[state]);
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
      fprintf(stderr, "failure (%d %s) connecting to wifi\n", err, esp_err_to_name(err));
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
    printf("got network address, connecting to mqtt\n");
    set_network_state(MQTT_CONNECTING);
    if((err = esp_mqtt_client_start(MQTTHandle)) != ESP_OK){
      fprintf(stderr, "failure (%d %s) connecting to mqtt\n", err, esp_err_to_name(err));
    }
  }else{
    fprintf(stderr, "unknown ip event %ld\n", id);
  }
}

static void
set_motor_pwm(void){
  set_pwm(MOTOR_CHAN, MotorPWM);
  // bring standby pin low if we're not sending any pwm, high otherwise
  if(MotorPWM == 0){
    gpio_set_level(MOTOR_SBYPIN, 0);
    printf("set motor standby low\n");
  }else{
    gpio_set_level(MOTOR_SBYPIN, 1);
    printf("set motor standby high\n");
  }
}

#define CCHAN "control/"
#define MPWM_CHANNEL CCHAN DEVICE "/mpwm"
#define LPWM_CHANNEL CCHAN DEVICE "/lpwm"
#define UPWM_CHANNEL CCHAN DEVICE "/upwm"

void handle_mqtt_msg(const esp_mqtt_event_t* e){
  printf("control message [%.*s] [%.*s]\n", e->topic_len, e->topic, e->data_len, e->data);
  if(strncmp(e->topic, MPWM_CHANNEL, e->topic_len) == 0 && e->topic_len == strlen(MPWM_CHANNEL)){
    int pwm = extract_pwm(e->data, e->data_len);
    if(pwm >= 0){
      MotorPWM = pwm;
      set_motor_pwm();
    }
  }else if(strncmp(e->topic, LPWM_CHANNEL, e->topic_len) == 0 && e->topic_len == strlen(LPWM_CHANNEL)){
    int pwm = extract_pwm(e->data, e->data_len);
    if(pwm >= 0){
      LowerPWM = pwm;
      set_pwm(LOWER_FANCHAN, LowerPWM);
    }
  }else if(strncmp(e->topic, UPWM_CHANNEL, e->topic_len) == 0 && e->topic_len == strlen(UPWM_CHANNEL)){
    int pwm = extract_pwm(e->data, e->data_len);
    if(pwm >= 0){
      UpperPWM = pwm;
      set_pwm(UPPER_FANCHAN, UpperPWM);
    }
  }else{
    fprintf(stderr, "unknown topic, ignoring message\n");
  }
}

void mqtt_event_handler(void* arg, esp_event_base_t base, int32_t id, void* data){
  if(id == MQTT_EVENT_CONNECTED){
    printf("connected to mqtt\n");
    set_network_state(MQTT_ESTABLISHED);
    int er;
    if( (er = esp_mqtt_client_subscribe(MQTTHandle, MPWM_CHANNEL, 0)) ){
      fprintf(stderr, "failure %d subscribing to mqtt mpwm topic\n", er);
    }
    if( (er = esp_mqtt_client_subscribe(MQTTHandle, LPWM_CHANNEL, 0)) ){
      fprintf(stderr, "failure %d subscribing to mqtt lpwm topic\n", er);
    }
    if( (er = esp_mqtt_client_subscribe(MQTTHandle, UPWM_CHANNEL, 0)) ){
      fprintf(stderr, "failure %d subscribing to mqtt upwm topic\n", er);
    }
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
  mdns_instance_name_set("Dire Dryer");
  mdns_service_add(NULL, "_http", "_tcp", 80, NULL, 0);
  mdns_service_instance_name_set("_http", "_tcp", "Dire Dryer webserver");
  return 0;
}

static esp_err_t
httpd_get_handler(httpd_req_t *req){
  const char resp[] = "URI GET Response"; // FIXME
  httpd_resp_send(req, resp, HTTPD_RESP_USE_STRLEN);
  return ESP_OK;
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
  if((err = httpd_register_uri_handler(&HTTPServ, &httpd_get)) != ESP_OK){
    fprintf(stderr, "failure (%s) preparing URI %s\n", esp_err_to_name(err), httpd_get.uri);
    return -1;
  }
  return 0;
}

int setup_network(void){
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
    fprintf(stderr, "failure %d (%s) initializing wifi\n", err, esp_err_to_name(err));
    return -1;
  }
  esp_event_handler_instance_t wid;
  if((err = esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_event_handler, NULL, &wid)) != ESP_OK){
    fprintf(stderr, "failure %d (%s) registering wifi events\n", err, esp_err_to_name(err));
    return -1;
  }
  esp_event_handler_instance_t ipd;
  if((err = esp_event_handler_instance_register(IP_EVENT, ESP_EVENT_ANY_ID, ip_event_handler, NULL, &ipd)) != ESP_OK){
    fprintf(stderr, "failure %d (%s) registering ip events\n", err, esp_err_to_name(err));
    return -1;
  }
  if((err = esp_mqtt_client_register_event(MQTTHandle, MQTT_EVENT_CONNECTED, mqtt_event_handler, NULL)) != ESP_OK){
    fprintf(stderr, "failure %d (%s) registering mqtt events\n", err, esp_err_to_name(err));
    return -1;
  }
  if((err = esp_mqtt_client_register_event(MQTTHandle, MQTT_EVENT_DATA, mqtt_event_handler, NULL)) != ESP_OK){
    fprintf(stderr, "failure %d (%s) registering mqtt events\n", err, esp_err_to_name(err));
    return -1;
  }
  if((err = esp_wifi_set_mode(WIFI_MODE_STA)) != ESP_OK){
    fprintf(stderr, "failure %d (%s) setting STA mode\n", err, esp_err_to_name(err));
    return -1;
  }
  if((err = esp_wifi_set_config(WIFI_IF_STA, &stacfg)) != ESP_OK){
    fprintf(stderr, "failure %d (%s) configuring wifi\n", err, esp_err_to_name(err));
    return -1;
  }
  if((err = esp_wifi_start()) != ESP_OK){
    fprintf(stderr, "failure %d (%s) starting wifi\n", err, esp_err_to_name(err));
    return -1;
  }
  setup_mdns(); // allow a failure
  // FIXME we currently get a "no slots" error when trying to load a URI
  // handler. until this is fixed, don't bail on error.
  setup_httpd();
  return 0;
}

// HX711
int setup_sensors(void){
  fprintf(stderr, "we don't yet have hx711 support\n");
  /* FIXME
  Load.begin(HX711_DT, HX711_SCK);
  Load.set_scale(LoadcellScale);
  */
  return 0;
}

// TB6612FNG
static int
setup_motor(gpio_num_t sbypin, gpio_num_t pwmpin, gpio_num_t pin1, gpio_num_t pin2){
  if(gpio_set_output(sbypin)){
    return -1;
  }
  if(gpio_set_output(pin1)){
    return -1;
  }
  if(gpio_set_output(pin2)){
    return -1;
  }
  if(initialize_pwm(MOTOR_CHAN, pwmpin, 490, LEDC_TIMER_3)){
    return -1;
  }
  set_motor_pwm();
  return 0;
}

static void
setup(void){
  // FIXME
  //static const failure_indication PreFailure = { 64, 64, 64 };
  //neopixelWrite(RGB_BUILTIN, PreFailure.r, PreFailure.g, PreFailure.b);
  printf("dankdryer v" VERSION "\n");
  if(!init_pstore()){
    if(!read_pstore()){
      UsePersistentStore = true;
    }
  }
  esp_err_t e = gpio_install_isr_service(ESP_INTR_FLAG_IRAM);
  if(e != ESP_OK){
    fprintf(stderr, "error (%s) installing isr service\n", esp_err_to_name(e));
    set_failure(&SystemError);
  }
  if(setup_esp32temp()){
    set_failure(&SystemError);
  }
  if(setup_fans(LOWER_PWMPIN, UPPER_PWMPIN, LOWER_TACHPIN, UPPER_TACHPIN)){
    set_failure(&SystemError);
  }
  if(setup_motor(MOTOR_SBYPIN, MOTOR_PWMPIN, MOTOR_1PIN, MOTOR_2PIN)){
    set_failure(&SystemError);
  }
  if(setup_sensors()){
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
  gpio_dump_io_configuration(stdout, SOC_GPIO_VALID_GPIO_MASK);
}

// we don't try to measure the first iteration, as we don't yet have a
// timestamp (and the fans are spinning up, anyway).
int getFanTachs(unsigned *lrpm, unsigned *urpm){
  static uint32_t m;
  int ret = -1;
  if(m){
    const int64_t diffu = esp_timer_get_time() - m;
    *lrpm = LowerPulses;
    *urpm = UpperPulses;
    printf("raw: %u %u\n", *lrpm, *urpm);
    *lrpm /= 2; // two pulses for each rotation
    *urpm /= 2;
    const float scale = 60.0 * 1000000u / diffu;
    printf("scale: %f diffu: %lld\n", scale, diffu);
    *lrpm *= scale;
    *urpm *= scale;
    ret = 0;
  }else{
    *lrpm = UINT_MAX;
    *urpm = UINT_MAX;
  }
  m = esp_timer_get_time();
  LowerPulses = 0;
  UpperPulses = 0;
  return ret;
}

void send_mqtt(int64_t curtime, float dtemp, unsigned lrpm, unsigned urpm,
               float weight){
  // FIXME check errors throughout!
  cJSON* root = cJSON_CreateObject();
  cJSON_AddNumberToObject(root, "uptimesec", curtime / 1000000ll);
  if(dtemp >= MIN_TEMP){
    cJSON_AddNumberToObject(root, "dtemp", dtemp);
  }
  // UINT_MAX is sentinel for known bad reading, but anything over 3KRPM on
  // these Noctua NF-A8 fans is indicative of error; they max out at 2500.
  if(lrpm < 3000){
    cJSON_AddNumberToObject(root, "lrpm", lrpm);
  }
  if(urpm < 3000){
    cJSON_AddNumberToObject(root, "urpm", urpm);
  }
  cJSON_AddNumberToObject(root, "lpwm", LowerPWM);
  cJSON_AddNumberToObject(root, "upwm", UpperPWM);
  if(weight >= 0 && weight < 5000){
    cJSON_AddNumberToObject(root, "load", weight);
  }
  cJSON_AddNumberToObject(root, "mpwm", MotorPWM);
  char* s = cJSON_Print(root);
  size_t slen = strlen(s);
  printf("MQTT: %s\n", s);
  if(esp_mqtt_client_publish(MQTTHandle, MQTTTOPIC, s, slen, 0, 0)){
    fprintf(stderr, "couldn't publish %zuB mqtt message\n", slen);
  }
  free(s);
}

void app_main(void){
  setup();
  while(1){
    float ambient = getAmbient();
    float weight = getWeight();
    printf("esp32 temp: %f weight: %f\n", ambient, weight);
    printf("pwm-l: %lu pwm-u: %lu\n", LowerPWM, UpperPWM);
    unsigned lrpm, urpm;
    if(!getFanTachs(&lrpm, &urpm)){
      printf("tach-l: %u tach-u: %u\n", lrpm, urpm);
    }
    int64_t curtime = esp_timer_get_time();
    send_mqtt(curtime, ambient, lrpm, urpm, weight);
    vTaskDelay(pdMS_TO_TICKS(15000));
  }
}
