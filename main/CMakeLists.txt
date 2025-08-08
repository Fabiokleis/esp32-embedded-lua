idf_component_register(SRCS "main.c" "info.c"
		    PRIV_REQUIRES nvs_flash spi_flash esp_driver_gpio wifi_sta
                    INCLUDE_DIRS ".")
