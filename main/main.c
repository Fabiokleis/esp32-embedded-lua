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
#include "esp_log.h"

#include "driver/gpio.h"

// lua 
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

#define LED_D2 2
#define HIGH 1
#define LOW 0

#include "wifi_sta.h"

static const char *TAG = "C_ESP32_RUNTIME";
static const char *LUA_TAG = "LUA_FUNCTION_RUNTIME";

/*
  1: recebe o PIN no primeiro arg da stack
  2: HIGH ou LOW do segundo arg da stack
*/
int lua_blink(lua_State *L) {
    ESP_LOGI(LUA_TAG, "running lua_blink");    
    int pin = luaL_checkinteger(L, 1);
    int level = luaL_checkinteger(L, 2);
    gpio_set_level(pin, level);
    ESP_LOGI(LUA_TAG, "returning lua_blink");    
    return 0;
}

/*
  1: recebe o PIN
  2: total de vezes que vai blinkar
  3: tempo em mili a cada blink
*/
int lua_blink_period(lua_State *L) {
    ESP_LOGI(LUA_TAG, "running lua_blink_period");    
    int pin = luaL_checkinteger(L, 1);
    int total = luaL_checkinteger(L, 2);
    int step = luaL_checkinteger(L, 3);
    for (int i = 0; i < total; ++i) {
         gpio_set_level(pin, LOW);   
         vTaskDelay(step / portTICK_PERIOD_MS);
	 gpio_set_level(pin, HIGH);
	 vTaskDelay(step / portTICK_PERIOD_MS);
    }
    ESP_LOGI(LUA_TAG, "returning lua_blink_period");    
    return 0;
}

void app_main(void)
{
    gpio_config_t io_config = {
      .mode = GPIO_MODE_OUTPUT,
      .pin_bit_mask = 1ULL << LED_D2,
    };
    gpio_config(&io_config);
    gpio_reset_pin(LED_D2);
    gpio_set_direction(LED_D2, GPIO_MODE_OUTPUT);


    ESP_LOGI(TAG, "ESP_WIFI_MODE_STA");
    wifi_init_sta();
    
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


    ESP_LOGI(TAG, "calling C functions from lua");

    lua_State *L = luaL_newstate();
    luaL_openlibs(L);

    const struct luaL_Reg funcs_gpio[] = {
        { "blink", lua_blink },
        {  "blink_period", lua_blink_period }
    };

    // We create a new table
    lua_newtable(L);

    luaL_setfuncs(L, funcs_gpio, 0);

    lua_setglobal(L, "gpio");

    char *code1 = "gpio.blink(2, 1)";

    ESP_LOGI(TAG, "running gpio.blink(2, 1)");
    if (luaL_dostring(L, code1) == LUA_OK) {
        lua_pop(L, lua_gettop(L));
    }
    
    char *code2 = "gpio.blink_period(2, 10, 500)";

    ESP_LOGI(TAG, "running gpio.blink_period(2, 10, 500)");
    if (luaL_dostring(L, code2) == LUA_OK) {
        lua_pop(L, lua_gettop(L));
    }

    printf("This is %s chip with %d CPU core(s), %s%s%s%s, ",
           CONFIG_IDF_TARGET,
           chip_info.cores,
           (chip_info.features & CHIP_FEATURE_WIFI_BGN) ? "WiFi/" : "",
           (chip_info.features & CHIP_FEATURE_BT) ? "BT" : "",
           (chip_info.features & CHIP_FEATURE_BLE) ? "BLE" : "",
           (chip_info.features & CHIP_FEATURE_IEEE802154) ? ", 802.15.4 (Zigbee/Thread)" : "");

    major_rev = chip_info.revision / 100;
    minor_rev = chip_info.revision % 100;
    printf("silicon revision v%d.%d, ", major_rev, minor_rev);
    if(esp_flash_get_size(NULL, &flash_size) != ESP_OK) {
        printf("Get flash size failed");
        return;
    }

    printf("%" PRIu32 "MB %s flash\n", flash_size / (uint32_t)(1024 * 1024),
           (chip_info.features & CHIP_FEATURE_EMB_FLASH) ? "embedded" : "external");

    printf("Minimum free heap size: %" PRIu32 " bytes\n", esp_get_minimum_free_heap_size());

    
    //vTaskDelay(10000 / portTICK_PERIOD_MS);
    lua_close(L);
    fflush(stdout);
    esp_restart();
}
