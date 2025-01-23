#ifndef DANKDRYER_PINS
#define DANKDRYER_PINS

// esp32-c6 mini-1u-h4  pin assignments

// https://docs.espressif.com/projects/esp-idf/en/stable/esp32c6/api-reference/peripherals/gpio.html
// 0--6 are connected to ADC1
#define SDA_PIN       GPIO_NUM_0   // I2C clock
#define SCL_PIN       GPIO_NUM_1   // I2C data
// 4 and 5 are strapping pins
#define UPPER_PWMPIN  GPIO_NUM_4   // upper chamber fan pwm
#define UPPER_TACHPIN GPIO_NUM_5   // upper chamber fan tach
#define THERM_DATAPIN GPIO_NUM_6   // analog thermometer (ADC1)
// 8 and 9 are strapping pins
// 12--13 are used for JTAG (not strictly needed)
#define MOTOR_GATEPIN GPIO_NUM_12   // MOSFET gate for motor
#define SSR_GPIN      GPIO_NUM_13  // heater solid state relay
// 15 is a strapping pin
#define LOWER_PWMPIN  GPIO_NUM_14  // lower chamber fan pwm
#define LOWER_TACHPIN GPIO_NUM_15  // lower chamber fan tach
#define HALL_DATAPIN  GPIO_NUM_19  // hall sensor
// 24--30 are reserved for SPI flash

#endif
