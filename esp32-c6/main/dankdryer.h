#ifndef DANKDRYER_DANKDRYER
#define DANKDRYER_DANKDRYER

#define NVS_HANDLE_NAME "pstore"

#include <nvs.h>
#include <stdbool.h>
#include <driver/gpio.h>
#include <mqtt_client.h>

int setup_intr(gpio_num_t pin, _Atomic(uint32_t)* arg);
int nvs_get_opt_u32(nvs_handle_t nh, const char* recname, uint32_t* val);
void handle_mqtt_msg(const esp_mqtt_event_t* e);

static inline const char*
bool_as_onoff(bool b){
  return b ? "on" : "off";
}

#endif
