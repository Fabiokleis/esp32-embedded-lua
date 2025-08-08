#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"

// esp
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

// wifi sta custom component
#include "wifi_sta.h"
#include "info.h"

static const char *TAG = "C_ESP32_RUNTIME";
static const char *LUA_TAG = "LUA_FUNCTION_RUNTIME";

/*
  1: recebe o PIN no primeiro arg da stack
  2: HIGH ou LOW do segundo arg da stack
*/
int lua_blink(lua_State *L) {
    ESP_LOGI(LUA_TAG, "called lua_blink");
    int pin = luaL_checkinteger(L, 1);
    int level = luaL_checkinteger(L, 2);
    gpio_set_level(pin, level);
    return 0;
}

/*
  1: recebe o PIN
  2: total de vezes que vai blinkar
  3: tempo em mili a cada blink
*/
int lua_blink_period(lua_State *L) {
    int pin = luaL_checkinteger(L, 1);
    int total = luaL_checkinteger(L, 2);
    int step = luaL_checkinteger(L, 3);
    for (int i = 0; i < total; ++i) {
         gpio_set_level(pin, LOW);   
         vTaskDelay(step / portTICK_PERIOD_MS);
	 gpio_set_level(pin, HIGH);
	 vTaskDelay(step / portTICK_PERIOD_MS);
    }
    return 0;
}

void setup_nvs() {
    //Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    ESP_LOGI(TAG, "NVS initialized");
}

void setup_gpio() {  
    gpio_config_t io_config = {
      .mode = GPIO_MODE_OUTPUT,
      .pin_bit_mask = 1ULL << LED_D2,
    };
    gpio_config(&io_config);
    gpio_reset_pin(LED_D2);
    gpio_set_direction(LED_D2, GPIO_MODE_OUTPUT);
    ESP_LOGI(TAG, "gpio configured");
}


void wifi_station_task() {
    log_memory_usage("wifi station task start");

    ESP_LOGI(TAG, "started ESP_WIFI_MODE_STA");
    wifi_init_sta();

    while (1) {
        ESP_LOGI(TAG, "running wifi task");
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}

void lua_vm_task(void *x_lua) {  
  
    lua_State *L = (lua_State *)x_lua;

    ESP_LOGI(TAG, "starting lua vm");

    L = luaL_newstate();

    if (NULL == L) {
        ESP_LOGE(TAG, "failed lua vm NULL");
	return;
    }

    luaL_checkversion(L);
    
    ESP_LOGI(TAG, "lua vm created");

    luaL_openlibs(L);

    ESP_LOGI(TAG, "lua libs open");

    
    const struct luaL_Reg funcs_gpio[] = {
      { "blink", lua_blink },
      { "blink_period", lua_blink_period },
      { NULL, NULL },
    };

    lua_newtable(L);
    luaL_setfuncs(L, funcs_gpio, 0);
    lua_setglobal(L, "gpio");
    
    ESP_LOGI(TAG, "created lua gpio table");

    log_memory_usage("lua vm task memory");
    
    char *high = "gpio.blink(2, 1)";
    char *low = "gpio.blink(2, 0)";
    
    /* char *code2 = "gpio.blink_period(2, 10, 500)"; */
    /* ESP_LOGI(LUA_TAG, "running gpio.blink_period(2, 10, 500)"); */
    /* if (luaL_dostring(L, code2) == LUA_OK) { */
    /*     lua_pop(L, lua_gettop(L)); */
    /* } */
    
    while (1) {
        ESP_LOGI(LUA_TAG, "running lua task");
	vTaskDelay(500 / portTICK_PERIOD_MS);
	if (luaL_dostring(L, high) == LUA_OK) {
	    lua_pop(L, lua_gettop(L));
	}
	vTaskDelay(500 / portTICK_PERIOD_MS);
	if (luaL_dostring(L, low) == LUA_OK) {
	    lua_pop(L, lua_gettop(L));
	}
    }
}

void app_main(void) {  

    ESP_LOGI(TAG, "start esp32 embedded lua");
    log_memory_usage("memory default");
    setup_gpio();
    setup_nvs();
    log_memory_usage("gpip & nvs memory usage");

    TaskHandle_t xwifi = NULL;
    TaskHandle_t xlua = NULL;
    lua_State *L = NULL;

    xTaskCreatePinnedToCore(&lua_vm_task, "lua_vm_task", 4096, L, 4, &xlua, 1);
    xTaskCreatePinnedToCore(&wifi_station_task, "wifi_sta_task", 4096, NULL, 3, &xwifi, 0);

    while (1) {
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }

    /* lua_close(L); */
    /* fflush(stdout); */

    /* if (xlua != NULL) { */
    /*     vTaskDelete(xlua); */
    /* } */
    
    /* if (xwifi != NULL) { */
    /*     vTaskDelete(xwifi); */
    /* } */
    
    /* esp_restart(); */
}
