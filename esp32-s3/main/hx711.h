#ifndef HX711_ESP_IDF
#define HX711_ESP_IDF

#include <stdint.h>
#include <driver/gpio.h>

// ESP-IDF component for working with Avia Semiconductor HX711 ADCs.

typedef struct HX711 {
  gpio_num_t dpin;
  gpio_num_t clockpin;
} HX711;

// successful return (zero) does not imply that the device was actually
// found or successfully used, but only that we set up the pins and
// initialized the HX711 structure.
int hx711_init(HX711 *hx, gpio_num_t dpin, gpio_num_t clockpin);

// read from the HX711. nonblocking, and will return error immediately
// if data is unavailable.
int hx711_read(HX711 *hx, int32_t* val);

#endif
