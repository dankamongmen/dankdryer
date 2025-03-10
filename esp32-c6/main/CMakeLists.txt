idf_component_register(SRCS "dankdryer.c" "fans.c" "networking.c" "ota.c" "dryer-network.h" "lcd.c" "networking.h" "pins.h"
                    PRIV_REQUIRES app_update bt driver esp_adc esp_app_format esp_driver_gpio esp_http_server esp_lcd esp_wifi json mqtt nvs_flash openthread spi_flash
                    INCLUDE_DIRS "")
