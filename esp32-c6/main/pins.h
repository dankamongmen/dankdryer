#ifndef DANKDRYER_PINS
#define DANKDRYER_PINS

// esp32-c6 mini-1u-h4  pin assignments

// https://docs.espressif.com/projects/esp-idf/en/stable/esp32c6/api-reference/peripherals/gpio.html
// 0--6 are connected to ADC1
#define SDA_PIN       GPIO_NUM_0   // I2C data
#define SCL_PIN       GPIO_NUM_1   // I2C clock
#define HALL_DATAPIN  GPIO_NUM_2   // hall sensor
#define THERM_DATAPIN GPIO_NUM_3   // analog thermometer (ADC1)
// 4 and 5 are strapping pins
#define MOTOR_GATEPIN GPIO_NUM_6   // MOSFET gate for motor
// 8 and 9 are strapping pins
// 12--13 are used for JTAG (not strictly needed)
#define SSR_GPIN      GPIO_NUM_14  // heater solid state relay
// 15 is a strapping pin
// 24--30 are reserved for SPI flash

#endif
