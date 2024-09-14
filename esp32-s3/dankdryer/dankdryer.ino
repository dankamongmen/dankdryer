// dankdryer firmware
// intended for use on an ESP32-S3-WROOM-1

#include <HX711.h>
#include <driver/temperature_sensor.h>

#define LOAD_DATAPIN 6
#define LOAD_CLOCKPIN 5

HX711 Load;
static temperature_sensor_handle_t temp;

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
  temperature_sensor_config_t conf = TEMPERATURE_SENSOR_CONFIG_DEFAULT(-10, 100);
  if(temperature_sensor_install(&conf, &temp)){
    printf("failed to set up thermostat\n");
    return -1;
  }
  if(temperature_sensor_enable(temp)){
    printf("failed to enable thermostat\n");
    return -1;
  }
  return 0;
}

void setup(void){
  Load.begin(LOAD_DATAPIN, LOAD_CLOCKPIN);
  setup_esp32temp();
}

void loop(void){
  float ambient = getAmbient();
  delay(1000);
}
