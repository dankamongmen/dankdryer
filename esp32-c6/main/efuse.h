#ifndef HOHLRAUM_EFUSE
#define HOHLRAUM_EFUSE

#include <stdint.h>

// eFuse provides a set of write-once 256-bit blocks, each composed of
// 8 32-bit registers. we only get one block for user data; we use the
// first two registers for the device's serial number, having the
// printed form LL-DDD-DDD-DDD where L is any capital letter and DDD
// is any number between 0 and 999 inclusive.
typedef struct device_id {
  char product[2];      // product ID (two letters)
  uint16_t electronics; // electronics version 0--999
  uint32_t serialnum;   // serial number 0--999999
} device_id;

// returns 0 on success, -1 on an invalid device id/serial.
int load_device_id(void);

uint32_t get_serial_num(void);

#endif
