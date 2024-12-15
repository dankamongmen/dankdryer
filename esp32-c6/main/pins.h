#ifndef DANKDRYER_PINS
#define DANKDRYER_PINS

// esp32-c6 mini-1u-h4  pin assignments

// https://docs.espressif.com/projects/esp-idf/en/stable/esp32c6/api-reference/peripherals/gpio.html
// 0--6 are connected to ADC1
#define SSR_GPIN GPIO_NUM_1        // heater solid state relay
#define THERM_DATAPIN GPIO_NUM_2   // analog thermometer (ADC1)
// 4 and 5 are strapping pins
// 8 and 9 are strapping pins
// 12--13 are used for JTAG (not strictly needed)
#define SDA_PIN GPIO_NUM_14        // I2C data
// 15 is a strapping pin
#define SCL_PIN GPIO_NUM_18        // I2C clock
#define HX711_DATAPIN GPIO_NUM_19  // hx711 data (input)
#define HX711_CLOCKPIN GPIO_NUM_20 // hx711 clock (output)
#define MOTOR_RELAY GPIO_NUM_22    // enable relay for motor
#define HALL_DATAPIN GPIO_NUM_23   // hall sensor
// 24--30 are reserved for SPI flash

#endif
