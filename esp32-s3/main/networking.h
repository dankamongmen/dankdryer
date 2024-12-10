#ifndef DANKDRYER_NETWORKING
#define DANKDRYER_NETWORKING

#include <nvs.h>
#include <stddef.h>
#include <stdbool.h>

#define VERSION "0.0.9"
#define DEVICE "dankdryer"

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
int nvs_get_opt_u32(nvs_handle_t nh, const char* recname, uint32_t* val);

#endif
