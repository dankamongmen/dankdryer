#ifndef DANKDRYER_PINS
#define DANKDRYER_PINS

#include <driver/gpio.h>

// esp32-c6 mini-1u-h4 pin assignments

// https://docs.espressif.com/projects/esp-idf/en/stable/esp32c6/api-reference/peripherals/gpio.html
// 0--6 are connected to ADC1
#define LOWER_TACHPIN GPIO_NUM_0   // lower chamber fan tach
#define LOWER_PWMPIN  GPIO_NUM_1   // lower chamber fan pwm
#define HALL_DATAPIN  GPIO_NUM_4   // hall sensor
#define THERM_DATAPIN GPIO_NUM_5   // analog thermometer (ADC1)
// 4 and 5 are strapping pins
#define UPPER_PWMPIN  GPIO_NUM_6   // upper chamber fan pwm
#define UPPER_TACHPIN GPIO_NUM_7   // upper chamber fan tach
// 8 and 9 are strapping pins
#define FRESET_PIN    GPIO_NUM_9   // factory reset when held for 5s
// 12--13 are used for JTAG (not strictly needed)
#define MOTOR_GATEPIN GPIO_NUM_12  // MOSFET gate for motor
#define SDA_PIN       GPIO_NUM_13  // I2C clock
#define SCL_PIN       GPIO_NUM_14  // I2C data
// 15 is a strapping pin
#define SSR_GPIN      GPIO_NUM_15  // heater solid state relay
/*
#define LCD_RST_PIN   GPIO_NUM_18
#define LCD_SCL_PIN   GPIO_NUM_19
#define LCD_SDA_PIN   GPIO_NUM_20
#define LCD_DC_PIN    GPIO_NUM_21
#define LCD_CS_PIN    GPIO_NUM_22
*/
// 24--30 are reserved for SPI flash

int gpio_setup(gpio_num_t pin, gpio_mode_t mode, const char *mstr);

static inline int
gpio_set_inputoutput_opendrain(gpio_num_t pin){
  return gpio_setup(pin, GPIO_MODE_INPUT_OUTPUT_OD, "input+output(od)");
}

static inline int
gpio_set_inputoutput(gpio_num_t pin){
  return gpio_setup(pin, GPIO_MODE_INPUT_OUTPUT, "input+output");
}

static inline int
gpio_set_input(gpio_num_t pin){
  return gpio_setup(pin, GPIO_MODE_INPUT, "input");
}

static inline int
gpio_set_output(gpio_num_t pin){
  return gpio_setup(pin, GPIO_MODE_OUTPUT, "output");
}

#endif
