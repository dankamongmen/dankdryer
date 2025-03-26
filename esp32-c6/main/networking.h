#ifndef DANKDRYER_NETWORKING
#define DANKDRYER_NETWORKING

#include <nvs.h>
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <soc/gpio_num.h>

int setup_network(void);
int handle_dry(unsigned sec, unsigned temp);
int extract_bool(const char* data, size_t dlen, bool* val);
int extract_pwm(const char* data, size_t dlen);
void set_heater(bool enabled);
void set_motor(bool enabled);
void set_lower_pwm(unsigned pwm);
void set_upper_pwm(unsigned pwm);
unsigned get_lower_pwm(void);
unsigned get_upper_pwm(void);
unsigned get_lower_rpm(void);
unsigned get_upper_rpm(void);
uint32_t get_spool_rpm(void);
float get_weight(void);
float get_tare(void);
float get_upper_temp(void);
float get_lower_temp(void);
unsigned long long get_dry_ends_at(void);
unsigned long get_target_temp(void);
bool get_motor_state(void);
bool get_heater_state(void);
void set_tare(void);
void factory_reset(void);
void mqtt_publish(const char *s);
int write_wifi_config(const unsigned char* essid, const unsigned char* psk,
                      uint32_t state);
int read_wifi_config(unsigned char* essid, size_t essidlen,
                     unsigned char* psk, size_t psklen,
                     int* setupstate);
int setup_lcd(gpio_num_t sda, gpio_num_t scl, gpio_num_t dc, gpio_num_t cs, gpio_num_t rst);

#endif
