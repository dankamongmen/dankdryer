// dankdryer firmware
// intended for use on an ESP32-S3-WROOM-1
#define VERSION "0.0.1"
#include <nvs.h>
#include <HX711.h>
#include <nvs_flash.h>
#include <driver/temperature_sensor.h>

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

// defaults, some of which can be configured.
static uint32_t LowerPWM = 128;
static uint32_t UpperPWM = 128;
static uint32_t TargetTemp = 80;
static uint32_t LowerPulses, UpperPulses; // tach signals recorded

void tach_isr(void* pulsecount){
  auto pc = static_cast<uint32_t*>(pulsecount);
  ++*pc;
}

float getAmbient(void){
  float t;
  if(temperature_sensor_get_celsius(temp, &t)){
    printf("failed acquiring temperature\n");
    return NAN;
  }
  return t;
}

// the esp32-s3 has a built in temperature sensor
int setup_esp32temp(void){
  temperature_sensor_config_t conf = TEMPERATURE_SENSOR_CONFIG_DEFAULT(-10, 80);
  if(temperature_sensor_install(&conf, &temp) != ESP_OK){
    printf("failed to set up thermostat\n");
    return -1;
  }
  if(temperature_sensor_enable(temp) != ESP_OK){
    printf("failed to enable thermostat\n");
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
    printf("failure (%d) initializing nvs!\n", err);
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
    printf("failure (%d) reading %s\n", err, recname);
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
    printf("failure (%d) opening nvs:" NVS_HANDLE_NAME "\n", err);
    return -1;
  }
  uint32_t bootcount;
#define BOOTCOUNT_RECNAME "bootcount"
  err = nvs_get_u32(nvsh, BOOTCOUNT_RECNAME, &bootcount);
  if(err && err != ESP_ERR_NVS_NOT_FOUND){
    printf("failure (%d) reading " NVS_HANDLE_NAME ":" BOOTCOUNT_RECNAME "\n", err);
    nvs_close(nvsh);
    return -1;
  }
  ++bootcount;
  printf("this is boot #%lu\n", bootcount);
  err = nvs_set_u32(nvsh, BOOTCOUNT_RECNAME, bootcount);
  if(err){
    printf("failure (%d) writing " NVS_HANDLE_NAME ":" BOOTCOUNT_RECNAME "\n", err);
    nvs_close(nvsh);
    return -1;
  }
#undef BOOTCOUNT_RECNAME
  err = nvs_commit(nvsh);
  if(err){
    printf("failure (%d) committing nvs:" NVS_HANDLE_NAME "\n", err);
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
  // FIXME set pins to INPUT
  return ret;
}

void setup(void){
  Serial.begin(115200);
  printf("dankdryer v" VERSION "\n");
  if(!init_pstore()){
    if(!read_pstore()){
      UsePersistentStore = true;
    }
  }
  Load.begin(LOAD_DATAPIN, LOAD_CLOCKPIN);
  if(setup_esp32temp()){
    // FIXME LED feedback
  }
  if(setup_fans(LOWER_PWMPIN, UPPER_PWMPIN, LOWER_TACHPIN, UPPER_TACHPIN)){
    // FIXME LED feedback
  }
  //gpio_dump_all_io_configuration(stdout, SOC_GPIO_VALID_GPIO_MASK);
  printf("initialization complete v" VERSION "\n");
}

void loop(void){
  float ambient = getAmbient();
  if(!isnan(ambient)){
    printf("esp32 temp: %f\n", ambient);
  }
  delay(1000);
}
