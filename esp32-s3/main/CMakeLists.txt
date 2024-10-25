idf_component_register(SRC_DIRS "." "HX711_IDF"
                    PRIV_REQUIRES driver esp_adc esp_driver_gpio esp_http_server esp_wifi json mqtt nvs_flash spi_flash
                    INCLUDE_DIRS "HX711_IDF/main")
