#include "emc230x.h"
#include <esp_log.h>
#include <stdint.h>
#include <freertos/FreeRTOS.h>

#define TIMEOUT_MS 35 // derived from SMBus

static const char* TAG = "emc";

#define EMC2301_ADDRESS   0x2f // emc2301
#define EMC2302_1_ADDRESS 0x2e // emc2302-1
#define EMC2302_2_ADDRESS 0x2f // emc2302-2 (same as emc2301)

typedef enum {
  EMCREG_CONFIGURATION = 0x20,
  EMCREG_FANSTATUS = 0x24,
  EMCREG_STALLSTATUS = 0x25,
  EMCREG_SPINSTATUS = 0x26,
  EMCREG_DRIVESTATUS = 0x27,
  EMCREG_FANINTR = 0x29,
  EMCREG_PWMPOLARITY = 0x2a,
  EMCREG_PWMOUTPUT = 0x2b,
  EMCREG_PWMBASE123 = 0x2d,
  EMCREG_FAN1SETTING = 0x30,
  EMCREG_PWM1DIVIDE = 0x31,
  EMCREG_FAN1CONF1 = 0x32,
  EMCREG_FAN1CONF2 = 0x33,
  EMCREG_GAIN1 = 0x35,
  EMCREG_FAN1SPINUP = 0x36,
  EMCREG_FAN1MAXSTEP = 0x37,
  EMCREG_FAN1MINDRIVE = 0x38,
  EMCREG_FAN1FAILLOW = 0x3a,
  EMCREG_FAN1FAILHIGH = 0x3b,
  EMCREG_TACH1TARGLOW = 0x3c,
  EMCREG_TACH1TARGHIGH = 0x3d,
  EMCREG_TACH1READLOW = 0x3e,
  EMCREG_TACH1READHIGH = 0x3f,
  // fan2 registers are only supported on emc230[235]
  EMCREG_FAN2SETTING = 0x40,
  EMCREG_PWM2DIVIDE = 0x41,
  EMCREG_FAN2CONF1 = 0x42,
  EMCREG_FAN2CONF2 = 0x43,
  EMCREG_GAIN2 = 0x45,
  EMCREG_FAN2SPINUP = 0x46,
  EMCREG_FAN2MAXSTEP = 0x47,
  EMCREG_FAN2MINDRIVE = 0x48,
  EMCREG_FAN2FAILLOW = 0x4a,
  EMCREG_FAN2FAILHIGH = 0x4b,
  EMCREG_TACH2TARGLOW = 0x4c,
  EMCREG_TACH2TARGHIGH = 0x4d,
  EMCREG_TACH2READLOW = 0x4e,
  EMCREG_TACH2READHIGH = 0x4f,
  // fan3 registers are only supported on emc230[35]
  EMCREG_FAN3SETTING = 0x50,
  EMCREG_PWM3DIVIDE = 0x51,
  EMCREG_FAN3CONF1 = 0x52,
  EMCREG_FAN3CONF2 = 0x53,
  EMCREG_GAIN3 = 0x55,
  EMCREG_FAN3SPINUP = 0x56,
  EMCREG_FAN3MAXSTEP = 0x57,
  EMCREG_FAN3MINDRIVE = 0x58,
  EMCREG_FAN3FAILLOW = 0x5a,
  EMCREG_FAN3FAILHIGH = 0x5b,
  EMCREG_TACH3TARGLOW = 0x5c,
  EMCREG_TACH3TARGHIGH = 0x5d,
  EMCREG_TACH3READLOW = 0x5e,
  EMCREG_TACH3READHIGH = 0x5f,
  // fan4 and fan5 registers are only supported on emc2305
  EMCREG_FAN4SETTING = 0x60,
  EMCREG_PWM4DIVIDE = 0x61,
  EMCREG_FAN4CONF1 = 0x62,
  EMCREG_FAN4CONF2 = 0x63,
  EMCREG_GAIN4 = 0x65,
  EMCREG_FAN4SPINUP = 0x66,
  EMCREG_FAN4MAXSTEP = 0x67,
  EMCREG_FAN4MINDRIVE = 0x68,
  EMCREG_FAN4FAILLOW = 0x6a,
  EMCREG_FAN4FAILHIGH = 0x6b,
  EMCREG_TACH4TARGLOW = 0x6c,
  EMCREG_TACH4TARGHIGH = 0x6d,
  EMCREG_TACH4READLOW = 0x6e,
  EMCREG_TACH4READHIGH = 0x6f,
  EMCREG_FAN5SETTING = 0x70,
  EMCREG_PWM5DIVIDE = 0x71,
  EMCREG_FAN5CONF1 = 0x72,
  EMCREG_FAN5CONF2 = 0x73,
  EMCREG_GAIN5 = 0x75,
  EMCREG_FAN5SPINUP = 0x76,
  EMCREG_FAN5MAXSTEP = 0x77,
  EMCREG_FAN5MINDRIVE = 0x78,
  EMCREG_FAN5FAILLOW = 0x7a,
  EMCREG_FAN5FAILHIGH = 0x7b,
  EMCREG_TACH5TARGLOW = 0x7c,
  EMCREG_TACH5TARGHIGH = 0x7d,
  EMCREG_TACH5READLOW = 0x7e,
  EMCREG_TACH5READHIGH = 0x7f,
  EMCREG_SOFTWARELOCK = 0xef,     // when set, some registers become readonly
  EMCREG_PRODFEATURES = 0xfc,     // only supported on emc230[35]
  EMCREG_PRODUCT = 0xfd,          // ought be some EMCPRODUCTID_230x value
  EMCREG_MANUFACTURER = 0xfe,     // ought be EMCMANUFACTURERID
  EMCREG_REVISION = 0xff,
} emcreg_e;

#define EMCPRODUCTID_2301 0x37
#define EMCPRODUCTID_2302 0x36
#define EMCPRODUCTID_2303 0x35
#define EMCPRODUCTID_2305 0x34
#define EMCMANUFACTURERID 0x5d

// get the single byte of some register into *val and returning 0.
// returns -1 on failure.
static inline int
emc230x_readreg(i2c_master_dev_handle_t i2c, emcreg_e reg,
                const char* regname, uint8_t* val){
  uint8_t r = reg;
  esp_err_t e;
  if((e = i2c_master_transmit_receive(i2c, &r, 1, val, 1, TIMEOUT_MS)) != ESP_OK){
    ESP_LOGE(TAG, "error (%s) requesting %s via I2C", esp_err_to_name(e), regname);
    return -1;
  }
  ESP_LOGD(TAG, "got %s: 0x%02x", regname, *val);
  return 0;
}

static inline int
emc230x_manufacturer(i2c_master_dev_handle_t i2c, uint8_t* val){
  return emc230x_readreg(i2c, EMCREG_MANUFACTURER, "ManufacturerID", val);
}

static inline int
emc230x_product(i2c_master_dev_handle_t i2c, uint8_t* val){
  return emc230x_readreg(i2c, EMCREG_PRODUCT, "ProductID", val);
}

// probe for an emc230[1235] at the specified address. only considered a
// success upon verification of the expected product ID.
static int
emc230x_mod_detect(i2c_master_bus_handle_t i2c, i2c_master_dev_handle_t* i2cemc,
                   uint8_t addr, uint8_t productid){
  esp_err_t e = i2c_master_probe(i2c, addr, TIMEOUT_MS);
  if(e != ESP_OK){
    ESP_LOGI(TAG, "no probe response at 0x%02x", addr);
    return -1;
  }
  ESP_LOGI(TAG, "got probe response at 0x%02x, checking for emc230x", addr);
  i2c_device_config_t devcfg = {
    .dev_addr_length = I2C_ADDR_BIT_LEN_7,
    .device_address = addr,
    .scl_speed_hz = 100000,
	};
  if((e = i2c_master_bus_add_device(i2c, &devcfg, i2cemc)) != ESP_OK){
    ESP_LOGE(TAG, "error (%s) adding i2c device at 0x%02x", esp_err_to_name(e), addr);
    return -1;
  }
  uint8_t regv;
  if(emc230x_manufacturer(*i2cemc, &regv) == 0){
    if(regv == EMCMANUFACTURERID){
      if(emc230x_product(*i2cemc, &regv) == 0){
        if(regv == productid){
          return 0;
        }
      }
    }
  }
  ESP_LOGE(TAG, "device at 0x%02x responded with unexpected data", addr);
  // the device didn't respond with the expected manufacturer/product ID.
  // remove it from the i2c bus master and return -1.
  if((e = i2c_master_bus_rm_device(*i2cemc)) != ESP_OK){
    ESP_LOGW(TAG, "error (%s) removing i2c device at 0x%02x", esp_err_to_name(e), addr);
  }
  return -1;
}

static int16_t
emc230x_detect_addr(i2c_master_bus_handle_t i2c, i2c_master_dev_handle_t* i2cemc,
                    emc230x_model model){
  switch(model){
    case EMC2301:
      if(emc230x_mod_detect(i2c, i2cemc, EMC2301_ADDRESS, EMCPRODUCTID_2301) == 0){
        return EMC2302_1_ADDRESS;
      }
      break;
    case EMC2302_MODEL_UNSPEC:
      if(emc230x_mod_detect(i2c, i2cemc, EMC2302_1_ADDRESS, EMCPRODUCTID_2302) == 0){
        return EMC2302_1_ADDRESS;
      }
      if(emc230x_mod_detect(i2c, i2cemc, EMC2302_2_ADDRESS, EMCPRODUCTID_2302) == 0){
        return EMC2302_2_ADDRESS;
      }
      break;
    case EMC2302_MODEL_1:
      if(emc230x_mod_detect(i2c, i2cemc, EMC2302_1_ADDRESS, EMCPRODUCTID_2302) == 0){
        return EMC2302_1_ADDRESS;
      }
      break;
    case EMC2302_MODEL_2:
      if(emc230x_mod_detect(i2c, i2cemc, EMC2302_2_ADDRESS, EMCPRODUCTID_2302) == 0){
        return EMC2302_2_ADDRESS;
      }
      break;
    case EMC2303:
      ESP_LOGE(TAG, "emc2303 is not yet supported"); // FIXME
      break;
    case EMC2305:
      ESP_LOGE(TAG, "emc2305 is not yet supported"); // FIXME
      break;
  }
  return -1;
}

int emc230x_detect(i2c_master_bus_handle_t i2c, i2c_master_dev_handle_t* i2cemc,
                   emc230x_model model){
  int16_t addr = emc230x_detect_addr(i2c, i2cemc, model);
  if(addr < 0){
    ESP_LOGE(TAG, "error detecting EMC230x");
    return -1;
  }
  ESP_LOGI(TAG, "successfully detected EMC230x at 0x%02x", addr);
  return 0;
}

// FIXME we'll probably want this to be async
static int
emc230x_xmit(i2c_master_dev_handle_t i2c, const void* buf, size_t blen){
  esp_err_t e = i2c_master_transmit(i2c, buf, blen, TIMEOUT_MS);
  if(e != ESP_OK){
    ESP_LOGE(TAG, "error %d transmitting %zuB via I2C", e, blen);
    return -1;
  }
  return 0;
}
