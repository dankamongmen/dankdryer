idf_component_register(SRCS "dankdryer.c" "emc2302.h" "hx711.c" "networking.c" "ota.c" "dryer-network.h" "networking.h" "pins.h"
                    PRIV_REQUIRES app_update bt driver esp_adc esp_app_format esp_driver_gpio esp_http_server esp_wifi json mqtt nvs_flash spi_flash
                    INCLUDE_DIRS "")
