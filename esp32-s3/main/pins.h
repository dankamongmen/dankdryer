#ifndef DANKDRYER_PINS
#define DANKDRYER_PINS

// esp32-s3 devkit-c pin assignments

// GPIO numbers (https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/api-reference/peripherals/gpio.html)
// 1--10 are connected to ADC1
// 0 and 3 are strapping pins
#define SSR_GPIN GPIO_NUM_1        // heater solid state relay
#define THERM_DATAPIN GPIO_NUM_2   // analog thermometer (ADC1)
#define LOWER_TACHPIN GPIO_NUM_8   // lower chamber fan tachometer
#define UPPER_TACHPIN GPIO_NUM_10  // upper chamber fan tachometer
// 11--20 are connected to ADC2, which is used by wifi
// (they can still be used as GPIOs; they cannot be used for adc)
#define UPPER_PWMPIN GPIO_NUM_11   // upper chamber fan speed
#define HX711_DATAPIN GPIO_NUM_12  // hx711 data (input)
#define HX711_CLOCKPIN GPIO_NUM_13 // hx711 clock (output)
#define LOWER_PWMPIN GPIO_NUM_18   // lower chamber fan speed
// 19--20 are used for JTAG (not strictly needed)
// 26--32 are used for pstore qspi flash
#define MOTOR_RELAY GPIO_NUM_35    // enable relay for motor
#define HALL_DATAPIN GPIO_NUM_36   // hall sensor
// 38 is sometimes the RGB neopixel
// 45 and 46 are strapping pins
#define RGB_PIN GPIO_NUM_48        // onboard RGB neopixel

#endif
