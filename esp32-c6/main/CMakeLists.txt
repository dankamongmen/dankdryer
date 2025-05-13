idf_component_register(SRCS "dankdryer.c"
                            "efuse.c" "efuse.h"
                            "fans.c"
                            "heater.c" "heater.h"
                            "lcd.c"
                            "networking.c" "networking.h"
                            "ota.c"
                            "pins.c" "pins.h"
                            "reset.c" "reset.h"
                            "version.h"
                    PRIV_REQUIRES app_update bt driver efuse esp_adc
                                  esp_app_format esp_driver_gpio esp_http_server
                                  esp_lcd esp_wifi json mqtt nvs_flash
                                  openthread spi_flash
                    INCLUDE_DIRS "")
