#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include <string.h>

// components
#include "driver/gpio.h"
#include "nvs_flash.h"
#include "mqtt_client.h"

// lua 
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

// wifi sta custom component
#include "portmacro.h"
#include "wifi_sta.h"
#include "info.h"

// esp

#include "esp_log.h"
#include "esp_event.h"
#include "esp_netif.h"

#define LED_D2 2
#define HIGH 1
#define LOW 0
#define MAX_CODE_SIZE 1024

static const char *TAG = "C_ESP32";
static const char *LUA_TAG = "LUA_VM_TASK";

static const char *MQTT_BROKER_URI = "mqtt://test.mosquitto.org";
static const char *TOPIC_PING = "0/lua_blinker/ping";
static const char *TOPIC_CODE = "0/lua_blinker/code/run";

static char CODE[MAX_CODE_SIZE]; // mqtt injected lua code


TaskHandle_t xlua = NULL; // lua notification
SemaphoreHandle_t x_wifi_connect;


/*
  1: recebe o tempo em mili de para da vTaskDelay
*/
int lua_sleep(lua_State *L) {
    ESP_LOGI(LUA_TAG, "called lua_sleep");
    int sleep_ms = luaL_checkinteger(L, 1);
    vTaskDelay(sleep_ms / portTICK_PERIOD_MS);
    return 0;
}

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
    if (NULL == x_wifi_connect) {
        ESP_LOGE(TAG, "failed to start wifi station task, x_wifi_connect NULL semphr");
        return;
    }
    
    log_memory_usage("wifi station task start");

    ESP_LOGI(TAG, "started ESP_WIFI_MODE_STA");
    wifi_init_sta();


    ESP_LOGI(TAG, "wifi connect give semphr");

    xSemaphoreGive(x_wifi_connect);
    while (1) {
        ESP_LOGI(TAG, "running wifi task");
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}

static void log_error_if_nonzero(const char *message, int error_code) {
    if (error_code != 0) {
        ESP_LOGE(TAG, "Last error %s: 0x%x", message, error_code);
    }
}

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data) {
    ESP_LOGD(TAG, "Event dispatched from event loop base=%s, event_id=%" PRIi32 "", base, event_id);
    esp_mqtt_event_handle_t event = event_data;
    esp_mqtt_client_handle_t client = event->client;
    int msg_id = 0;
    switch ((esp_mqtt_event_id_t)event_id) {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
	msg_id = esp_mqtt_client_subscribe(client, TOPIC_CODE, 0);
	ESP_LOGI(TAG, "subscribe %s msg_id %d", TOPIC_CODE, msg_id);
        break;
    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
        break;
    case MQTT_EVENT_SUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d, return code=0x%02x ", event->msg_id, (uint8_t)*event->data);
        break;
    case MQTT_EVENT_UNSUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_PUBLISHED:
        ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_DATA:
        ESP_LOGI(TAG, "MQTT_EVENT_DATA");
        printf("TOPIC=%.*s\r\n", event->topic_len, event->topic);
        printf("DATA=%.*s\r\n", event->data_len, event->data);
	
	if (event->data_len >= MAX_CODE_SIZE) break;
	
	memset(CODE, '\0', MAX_CODE_SIZE); // reseta o buffer
	memcpy(CODE, event->data, event->data_len); // copia o data recebido via mqtt para o codigo lua da task da vm
	xTaskNotify(xlua, 0, eNoAction); // envia uma notificacao para task da vm lua
	ESP_LOGI(TAG, "refreshed lua code %s", CODE);
        break;
    case MQTT_EVENT_ERROR:
        ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
        if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT) {
            log_error_if_nonzero("reported from esp-tls", event->error_handle->esp_tls_last_esp_err);
            log_error_if_nonzero("reported from tls stack", event->error_handle->esp_tls_stack_err);
            log_error_if_nonzero("captured as transport's socket errno",  event->error_handle->esp_transport_sock_errno);
            ESP_LOGI(TAG, "Last errno string (%s)", strerror(event->error_handle->esp_transport_sock_errno));

        }
        break;
    default:
        ESP_LOGI(TAG, "Other event id:%d", event->event_id);
        break;
    }
}

void mqtt_task() {
    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = MQTT_BROKER_URI,
    };

    // pega o semphr de wifi
    if (xSemaphoreTake(x_wifi_connect, portMAX_DELAY) == pdTRUE) {
        xSemaphoreGive(x_wifi_connect); // libera
	vSemaphoreDelete(x_wifi_connect); // deleta
    } else {
        ESP_LOGE(TAG, "failed to take x_wifi_connect semphr");
	return;
    }
    
    esp_mqtt_client_handle_t client = esp_mqtt_client_init(&mqtt_cfg);
    /* The last argument may be used to pass data to the event handler, in this example mqtt_event_handler */
    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_mqtt_client_start(client);

    ESP_LOGI(TAG, "mqtt started using %s", mqtt_cfg.broker.address.uri);

    log_memory_usage("mqtt memory after start");
    char *msg = "PINGU";

    int msg_id = 0;
    while (1) {
        vTaskDelay(1000 / portTICK_PERIOD_MS);
	msg_id = esp_mqtt_client_publish(client, TOPIC_PING, msg, strlen(msg), 0, 0);
	if (msg_id != -1) {
  	    ESP_LOGI(TAG, "pub message msg_id %d: %s", msg_id, msg);
	} else {
	    ESP_LOGE(TAG, "failed to pub message msg_id %d: %s", msg_id, msg);
	}
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
      { "sleep", lua_sleep },
      { "blink", lua_blink },
      { "blink_period", lua_blink_period },
      { NULL, NULL },
    };

    lua_newtable(L);
    luaL_setfuncs(L, funcs_gpio, 0);
    lua_setglobal(L, "gpio");
    
    ESP_LOGI(TAG, "created lua gpio table");

    log_memory_usage("lua vm task memory");
    
    while (1) {
        ESP_LOGI(LUA_TAG, "running lua task waiting notification");
	ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
	ESP_LOGI(LUA_TAG, "running lua code %s", CODE);
	if (luaL_dostring(L, CODE) == LUA_OK) {
	  lua_pop(L, lua_gettop(L));
	}
    }
}

void app_main(void) {  

    ESP_LOGI(TAG, "start esp32 embedded lua");
    log_memory_usage("memory default");
    setup_gpio();
    setup_nvs();

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    
    log_memory_usage("gpip & nvs memory usage");

    TaskHandle_t xwifi = NULL;
    TaskHandle_t xmqtt = NULL;
    lua_State *L = NULL;

    x_wifi_connect = xSemaphoreCreateBinary();
    
    if (NULL == x_wifi_connect) {
        ESP_LOGE(TAG, "failed to allocate semphr for wifi connection");
        return;
    }

    xTaskCreatePinnedToCore(&wifi_station_task, "wifi_sta_task", 4096, NULL, 3, &xwifi, 0);
    xTaskCreatePinnedToCore(&mqtt_task, "mqtt_task", 4096, NULL, 3, &xmqtt, 1);
    xTaskCreate(&lua_vm_task, "lua_vm_task", 4096, L, 4, &xlua);

    while (1) {
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }

    //failed:
    if (NULL != xwifi) vTaskDelete(xwifi);
    if (NULL != xmqtt) vTaskDelete(xmqtt);
    if (NULL != L) lua_close(L);
    if (NULL != xlua) vTaskDelete(xlua);
	
    /* esp_restart(); */
}
