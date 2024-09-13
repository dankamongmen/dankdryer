// dankdryer firmware
// intended for use on an ESP32-S3-WROOM-1

#include <HX711.h>

HX711 Load;

#define LOAD_DATAPIN 42
#define LOAD_CLOCKPIN 41

void setup(void){
  Load.begin(LOAD_DATAPIN, LOAD_CLOCKPIN);
}

void loop(void){
}
