idf_component_register(SRCS "dankdryer.c" "hx711.c" "ota.c"
                    PRIV_REQUIRES driver esp_adc esp_app_format esp_driver_gpio esp_http_server esp_wifi json mqtt nvs_flash spi_flash
                    INCLUDE_DIRS "")
