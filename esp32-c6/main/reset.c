#include "reset.h"
#include <stdatomic.h>

#define FRESET_HOLD_TIME_US 5000000ul
static _Atomic(int64_t) last_low_start = -1;

int setup_factory_reset(gpio_num_t pin){
  // FIXME
  return 0;
}

bool check_factory_reset(int64_t curtime){
  int64_t lls = last_low_start;
  if(lls < 0 || curtime < 0){
    return false;
  }
  if(curtime - lls < FRESET_HOLD_TIME_US){
    return false;
  }
  return true;
}
