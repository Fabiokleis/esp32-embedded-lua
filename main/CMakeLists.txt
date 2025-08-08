idf_component_register(SRCS "main.c"
		    PRIV_REQUIRES spi_flash esp_driver_gpio wifi_sta
                    INCLUDE_DIRS ".")
