idf_component_register(SRCS "main.c" "info.c"
		    PRIV_REQUIRES nvs_flash esp_driver_gpio esp_netif wifi_sta mqtt
                    INCLUDE_DIRS ".")
