# esp32-embedded-lua

brincando com a vm de lua embedada no esp32.


tutorial de como criar interface com vm a lua:
https://lucasklassmann.com/blog/2019-02-02-embedding-lua-in-c

documentacao da linguagem lua: 
https://www.lua.org/manual/5.3/manual.html

## como executar codigo dinamico na vm lua
```c
#define MAX_CODE_SIZE 1024

static const char *TAG = "C_ESP32";
static const char *LUA_TAG = "LUA_VM_TASK";

static const char *MQTT_BROKER_URI = "mqtt://test.mosquitto.org";
static const char *TOPIC_PING = "0/lua_blinker/ping";
static const char *TOPIC_CODE = "0/lua_blinker/code/run";

static char CODE[MAX_CODE_SIZE]; // mqtt injected lua code

TaskHandle_t xlua = NULL; // lua notification

/* .... */

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data) {
    /* .... */
    switch ((esp_mqtt_event_id_t)event_id) {
    /* .... */
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
}

/* .... */

// interface C para o lua
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
/* ... */
void lua_vm_task(void *x_lua) {  
    lua_State *L = (lua_State *)x_lua; // recebe via arg da task um ponteiro da vm lua
    ESP_LOGI(TAG, "starting lua vm");
    L = luaL_newstate(); // cria uma nova vm
    if (NULL == L) {
        ESP_LOGE(TAG, "failed lua vm NULL");
	      return;
    }
    luaL_checkversion(L);    
    ESP_LOGI(TAG, "lua vm created");
    luaL_openlibs(L); // abre as bibliotecas padrao do lua
    ESP_LOGI(TAG, "lua libs open");
    /* define uma biblioteca com as funcoes C que podem ser chamadas do lua */
    const struct luaL_Reg funcs_gpio[] = {
      { "sleep", lua_sleep },
      { "blink", lua_blink },
      { "blink_period", lua_blink_period },
      { NULL, NULL },
    };
    lua_newtable(L);
    luaL_setfuncs(L, funcs_gpio, 0);
    lua_setglobal(L, "gpio"); // a biblioteca tem o nome gpio globalmente
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
```
<img width="801" height="182" alt="image" src="https://github.com/user-attachments/assets/05957fdb-053b-429a-82dd-c36b0c355484" />

## components

espressif component lua: 
https://components.espressif.com/components/georgik/lua/

exemplo de como usar:
https://github.com/georgik/esp32-lua-example


