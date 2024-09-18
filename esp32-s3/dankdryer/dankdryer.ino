// dankdryer firmware
// intended for use on an ESP32-S3-WROOM-1

#include <nvs.h>
#include <HX711.h>
#include <nvs_flash.h>
#include <driver/temperature_sensor.h>

#define LOWER_PWMPIN 4
#define UPPER_PWMPIN 5
#define MOTOR_PWMPIN 35
#define UPPER_TACHPIN 36
#define LOWER_TACHPIN 37
// 38 is used for RGB LED
#define LOAD_CLOCKPIN 41
#define LOAD_DATAPIN 42
#define MOTOR_1PIN 47
#define MOTOR_2PIN 48

HX711 Load;
static bool UsePersistentStore; // set true upon successful initialization
static temperature_sensor_handle_t temp;

// defaults, some of which can be configured.
static unsigned LowerPWM = 128;
static unsigned UpperPWM = 128;
static unsigned TargetTemp = 80;

float getAmbient(void){
  float t;
  if(temperature_sensor_get_celsius(temp, &t)){
    printf("failed acquiring temperature\n");
    return NAN;
  }
  printf("esp32 temp: %f\n", t);
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

// read and update boot count, read configurable defaults from pstore if they
// are present (we do not write defaults to pstore, so we can differentiate
// between defaults and a configured value).
int read_pstore(void){
  return 0;
}

void setup(void){
  Serial.begin(115200);
  if(!init_pstore()){
    if(!read_pstore()){
      UsePersistentStore = true;
    }
  }
  Load.begin(LOAD_DATAPIN, LOAD_CLOCKPIN);
  if(setup_esp32temp()){
    // FIXME LED feedback
  }
}

void loop(void){
  float ambient = getAmbient();
  delay(1000);
}
