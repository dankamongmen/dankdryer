#ifndef DANKDRYER_FANS
#define DANKDRYER_FANS

#include <nvs.h>
#include <stdint.h>
#include <soc/gpio_num.h>

#ifdef LEDC_HIGH_SPEED_MODE
#define LEDCMODE LEDC_HIGH_SPEED_MODE
#else
#define LEDCMODE LEDC_LOW_SPEED_MODE
#endif
#define MAXPWMDUTY 255
#define LOWERPWM_RECNAME "lpwm"
#define UPPERPWM_RECNAME "upwm"

int setup_fans(gpio_num_t lowerppin, gpio_num_t upperppin,
               gpio_num_t lowertpin, gpio_num_t uppertpin);
int read_fans_pstore(nvs_handle_t nvsh);
void set_lower_pwm(unsigned pwm);
void set_upper_pwm(unsigned pwm);
unsigned get_lower_pwm(void);
unsigned get_upper_pwm(void);
uint32_t get_lower_tach(void);
uint32_t get_upper_tach(void);

#endif
