
/*
 * SPDX-FileCopyrightText: 2010-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include "info.h"
#include "esp_log.h"
#include "esp_heap_caps.h"

static const char *TAG = "memory debug";

void log_memory_usage(const char *message) {
    ESP_LOGI(TAG, "Free heap: %d, Min free heap: %d, Largest free block: %d, %s",
             heap_caps_get_free_size(MALLOC_CAP_DEFAULT),
             heap_caps_get_minimum_free_size(MALLOC_CAP_DEFAULT),
             heap_caps_get_largest_free_block(MALLOC_CAP_DEFAULT),
             message);
}
