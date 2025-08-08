#include <stdio.h>

/*
 * SPDX-FileCopyrightText: 2010-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include <stdio.h>
#include <inttypes.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_chip_info.h"
#include "esp_flash.h"
#include "esp_system.h"
#include "driver/gpio.h"

// lua 
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

#define LED_D2 2



void app_main(void)
{
    gpio_config_t io_config = {
      .mode = GPIO_MODE_OUTPUT,
      .pin_bit_mask = 1ULL << LED_D2,
    };
    gpio_config(&io_config);
    gpio_reset_pin(LED_D2);
    gpio_set_direction(LED_D2, GPIO_MODE_OUTPUT);
    
    /* Print chip information */
    esp_chip_info_t chip_info;
    uint32_t flash_size;
    esp_chip_info(&chip_info);
    printf("This is %s chip with %d CPU core(s), %s%s%s%s, ",
           CONFIG_IDF_TARGET,
           chip_info.cores,
           (chip_info.features & CHIP_FEATURE_WIFI_BGN) ? "WiFi/" : "",
           (chip_info.features & CHIP_FEATURE_BT) ? "BT" : "",
           (chip_info.features & CHIP_FEATURE_BLE) ? "BLE" : "",
           (chip_info.features & CHIP_FEATURE_IEEE802154) ? ", 802.15.4 (Zigbee/Thread)" : "");

    unsigned major_rev = chip_info.revision / 100;
    unsigned minor_rev = chip_info.revision % 100;
    printf("silicon revision v%d.%d, ", major_rev, minor_rev);
    if(esp_flash_get_size(NULL, &flash_size) != ESP_OK) {
        printf("Get flash size failed");
        return;
    }

    printf("%" PRIu32 "MB %s flash\n", flash_size / (uint32_t)(1024 * 1024),
           (chip_info.features & CHIP_FEATURE_EMB_FLASH) ? "embedded" : "external");

    printf("Minimum free heap size: %" PRIu32 " bytes\n", esp_get_minimum_free_heap_size());

    for (int i = 10; i >= 0; i--) {
        printf("Restarting in %d seconds...\n", i);
	gpio_set_level(LED_D2, 0);
	vTaskDelay(500 / portTICK_PERIOD_MS);
	gpio_set_level(LED_D2, 1);
        vTaskDelay(500 / portTICK_PERIOD_MS);
    }
    
    fflush(stdout);


    lua_State *L = luaL_newstate();
    luaL_openlibs(L);

    // Our Lua code, it simply prints a Hello, World message
    char * code = "print('Hello, World')";

    // Here we load the string and use lua_pcall for run the code
    if (luaL_loadstring(L, code) == LUA_OK) {
        if (lua_pcall(L, 0, 0, 0) == LUA_OK) {
            // If it was executed successfuly we 
            // remove the code from the stack
            lua_pop(L, lua_gettop(L));
        }
    }

    lua_close(L);
    fflush(stdout);

    esp_restart();
}
