// dankdryer firmware
// intended for use on an ESP32-S3-WROOM-1
#define VERSION "0.0.1"
#include "dryer-network.h"
#include <lwip/netif.h>
#include <nvs.h>
#include <HX711.h>
#include <esp_wifi.h>
#include <esp_netif.h>
#include <nvs_flash.h>
#include <mqtt_client.h>
#include <driver/ledc.h>
#include <hal/ledc_types.h>
#include <driver/temperature_sensor.h>

#define FANPWM_BIT_NUM LEDC_TIMER_8_BIT
#define RPMMAX (1u << 14u)

// GPIO numbers (https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/api-reference/peripherals/gpio.html)
// 0 and 3 are strapping pins
#define LOWER_PWMPIN GPIO_NUM_4
#define UPPER_PWMPIN GPIO_NUM_5
#define THERM_DATAPIN GPIO_NUM_8
// 19--20 are used for JTAG (not strictly needed)
// 26--32 are used for pstore qspi flash
#define MOTOR_PWMPIN GPIO_NUM_35
#define UPPER_TACHPIN GPIO_NUM_36
#define LOWER_TACHPIN GPIO_NUM_37
// 38 is used for RGB LED
#define LOAD_CLOCKPIN GPIO_NUM_41
#define LOAD_DATAPIN GPIO_NUM_42
// 45 and 46 are strapping pins
#define MOTOR_1PIN GPIO_NUM_47
#define MOTOR_2PIN GPIO_NUM_48

#define NVS_HANDLE_NAME "pstore"

HX711 Load;
static bool UsePersistentStore; // set true upon successful initialization
static temperature_sensor_handle_t temp;
static const ledc_channel_t LOWER_FANCHAN = LEDC_CHANNEL_0;
static const ledc_channel_t UPPER_FANCHAN = LEDC_CHANNEL_1;
static const ledc_mode_t LEDCMODE = LEDC_LOW_SPEED_MODE; // no high-speed on S3

// defaults, some of which can be configured.
static uint32_t LowerPWM = 128;
static uint32_t UpperPWM = 128;
static uint32_t TargetTemp = 80;
static uint32_t LowerPulses, UpperPulses; // tach signals recorded
static esp_mqtt_client_handle_t MQTTHandle;

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

static enum {
  WIFI_INVALID,
  WIFI_CONNECTING,
  MQTT_CONNECTING,
  MQTT_ESTABLISHED,
  NETWORK_STATE_COUNT
} NetworkState;

// ---------------------------------------------------------
// needed with 3.0.3, see https://github.com/espressif/arduino-esp32/issues/10084 
// FIXME remove this garbage
extern "C" int lwip_hook_ip6_input(struct pbuf *p, struct netif *inp) __attribute__((weak));
extern "C" int lwip_hook_ip6_input(struct pbuf *p, struct netif *inp) {
  if (ip6_addr_isany_val(inp->ip6_addr[0].u_addr.ip6)) {
    // We don't have an LL address -> eat this packet here, so it won't get accepted on input netif
    pbuf_free(p);
    return 1;
  }
  return 0;
}
// ---------------------------------------------------------

void tach_isr(void* pulsecount){
  auto pc = static_cast<uint32_t*>(pulsecount);
  ++*pc;
}

static inline bool valid_pwm_p(int pwm){
  return pwm >= 0 && pwm <= 255;
}

// precondition: isxdigit(c) is true
byte get_hex(char c){
  if(isdigit(c)){
    return c - '0';
  }
  c = tolower(c);
  return c - 'a' + 10;
}

// FIXME handle base 10 numbers as well (can we use strtoul?)
int extract_pwm(const String& payload){
  if(payload.length() != 2){
    printf("pwm wasn't 2 characters\n");
    return -1;
  }
  char h = payload.charAt(0);
  char l = payload.charAt(1);
  if(!isxdigit(h) || !isxdigit(l)){
    printf("invalid hex character\n");
    return -1;
  }
  byte hb = get_hex(h);
  byte lb = get_hex(l);
  // everything was valid
  return hb * 16 + lb;
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

float getAmbient(void){
  float t;
  if(temperature_sensor_get_celsius(temp, &t)){
    fprintf(stderr, "failed acquiring temperature\n");
    return NAN;
  }
  return t;
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
  // FIXME check any defaults we read and ensure they're sane
  // FIXME need we check for error here?
  nvs_close(nvsh);
  return 0;
}

int initialize_pwm(ledc_channel_t channel, gpio_num_t pin, int freq, ledc_timer_t timer){
  pinMode(pin, OUTPUT);
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
  printf("success!\n");
  return 0;
}

int initialize_25k_pwm(ledc_channel_t channel, gpio_num_t pin, ledc_timer_t timer){
  return initialize_pwm(channel, pin, 25000, timer);
}

int setup_fans(gpio_num_t lowerppin, gpio_num_t upperppin,
               gpio_num_t lowertpin, gpio_num_t uppertpin){
  int ret = 0;
  esp_err_t err;
  // FIXME set up 25K PWM / OUTPUT
  if( (err = gpio_install_isr_service(0))){ // FIXME check flags
    fprintf(stderr, "failure (%d) installing ISR service\n", err);
    return -1;
  }
  if( (err = gpio_isr_handler_add(lowertpin, tach_isr, &LowerPulses)) ){
    fprintf(stderr, "failure (%d) installing tach isr to %d\n", err, lowertpin);
    ret = -1; // don't exit with error
  }
  if( (err = gpio_isr_handler_add(uppertpin, tach_isr, &UpperPulses)) ){
    fprintf(stderr, "failure (%d) installing tach isr to %d\n", err, uppertpin);
    ret = -1; // don't exit with error
  }
  // FIXME set tach pins to INPUT
  initialize_25k_pwm(LOWER_FANCHAN, lowerppin, LEDC_TIMER_1);
  initialize_25k_pwm(UPPER_FANCHAN, upperppin, LEDC_TIMER_2);
  set_pwm(LOWER_FANCHAN, LowerPWM);
  set_pwm(UPPER_FANCHAN, UpperPWM);
  return ret;
}

int setup_network(void){
  const wifi_init_config_t wificfg = {
  };
  wifi_config_t stacfg = {
    .sta = {
      .ssid = WIFIESSID,
      .password = WIFIPASS,
    },
  };
  if((MQTTHandle = esp_mqtt_client_init(&MQTTConfig)) == NULL){
    fprintf(stderr, "couldn't create mqtt client\n");
    return -1;
  }
  esp_err_t err;
  if((err = esp_netif_init()) != ESP_OK){
    fprintf(stderr, "failure %d (%s) initializing tcp/ip\n", err, esp_err_to_name(err));
    goto bail;
  }
  if((err = esp_wifi_init(&wificfg)) != ESP_OK){
    fprintf(stderr, "failure %d (%s) initializing wifi\n", err, esp_err_to_name(err));
    goto bail;
  }
  esp_wifi_set_mode(WIFI_MODE_STA);
  if((err = esp_wifi_set_config(WIFI_IF_STA, &stacfg)) != ESP_OK){
    fprintf(stderr, "failure %d (%s) configuring wifi\n", err, esp_err_to_name(err));
    goto bail;
  }
  return 0;

bail:
  esp_mqtt_client_destroy(MQTTHandle);
  return -1;
}

typedef struct failure_indication {
  int r, g, b;
} failure_indication;

static bool StartupFailure;
static const failure_indication PreFailure = { RGB_BRIGHTNESS, RGB_BRIGHTNESS, RGB_BRIGHTNESS };
static const failure_indication SystemError = { RGB_BRIGHTNESS, 0, 0 };
static const failure_indication NetworkError = { RGB_BRIGHTNESS, 0, RGB_BRIGHTNESS };
static const failure_indication PostFailure = { 0, 0, 0 };

void set_led(const struct failure_indication *nin){
  neopixelWrite(RGB_BUILTIN, nin->r, nin->g, nin->b);
}

void set_failure(const struct failure_indication *fin){
  StartupFailure = true;
  set_led(fin);
}

static const failure_indication NetworkIndications[NETWORK_STATE_COUNT] = {
  { 0, 0, 0 },
  { 0, 64, 0 },
  { 0, 128, 0 },
  { 0, 255, 0 }
};

void setup(void){
  neopixelWrite(RGB_BUILTIN, PreFailure.r, PreFailure.g, PreFailure.b);
  Serial.begin(115200);
  printf("dankdryer v" VERSION "\n");
  if(!init_pstore()){
    if(!read_pstore()){
      UsePersistentStore = true;
    }
  }
  esp_event_loop_args_t elargs = {
    .queue_size = 8,
    .task_name = NULL,
  };
  esp_event_loop_handle_t elhandle = {
  };
  if(esp_event_loop_create(&elargs, &elhandle)){
    set_failure(&SystemError);
  }
  Load.begin(LOAD_DATAPIN, LOAD_CLOCKPIN);
  if(setup_esp32temp()){
    set_failure(&SystemError);
  }
  if(setup_fans(LOWER_PWMPIN, UPPER_PWMPIN, LOWER_TACHPIN, UPPER_TACHPIN)){
    set_failure(&SystemError);
  }
  if(setup_network()){
    set_failure(&NetworkError);
  }
  if(!StartupFailure){
    set_failure(&PostFailure);
  }
  //gpio_dump_all_io_configuration(stdout, SOC_GPIO_VALID_GPIO_MASK);
  printf("initialization complete v" VERSION "\n");
}

void handle_network(void){
  esp_err_t err;
  if(NetworkState != WIFI_INVALID){
    set_led(&NetworkIndications[NetworkState]);
  }
  printf("netstate: %d\n", NetworkState);
  switch(NetworkState){
    case WIFI_INVALID:
      if((err = esp_wifi_start()) != ESP_OK){
        fprintf(stderr, "failure (%d %s) starting wifi\n", err, esp_err_to_name(err));
      }else{
        NetworkState = WIFI_CONNECTING;
      }
      break;
    case WIFI_CONNECTING:
      break;
    case MQTT_CONNECTING:
      break;
    case MQTT_ESTABLISHED:
      break;
    case NETWORK_STATE_COUNT:
    default:
      // FIXME?
      break;
  }
  if(NetworkState != WIFI_INVALID){
    set_led(&NetworkIndications[NetworkState]);
  }
}

void loop(void){
  handle_network();
  float ambient = getAmbient();
  if(!isnan(ambient)){
    printf("esp32 temp: %f\n", ambient);
  }
  delay(1000);
}
